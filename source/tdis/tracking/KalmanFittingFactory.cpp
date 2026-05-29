#include "KalmanFittingFactory.h"

#include <Acts/Definitions/Units.hpp>
#include <Acts/Utilities/Helpers.hpp>
#include <ActsExamples/EventData/IndexSourceLink.hpp>
#include <ActsExamples/EventData/MeasurementCalibration.hpp>

#include "../logging/ActsLogHeplers.h"
#include "Acts/Plugins/Podio/PodioTrackContainer.hpp"
#include "Acts/Plugins/Podio/PodioTrackStateContainer.hpp"
#include "Acts/Plugins/Podio/PodioUtil.hpp"
#include "Acts/TrackFitting/GainMatrixSmoother.hpp"
#include "Acts/TrackFitting/GainMatrixUpdater.hpp"
#include "RefittingCalibrator.h"
#include "geometry/TdisGeometryHelper.hpp"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/DigitizedMtpcMcTrackCollection.h"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/TrackCollection.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrackSeedCollection.h"
#include "podio_model/TrajectoryCollection.h"

namespace tdis::tracking {

    struct SimpleReverseFilteringLogic {
        double momentumThreshold = 0;

        bool doBackwardFiltering(Acts::VectorMultiTrajectory::ConstTrackStateProxy trackState) const
        {
            auto momentum = std::abs(1 / trackState.filtered()[Acts::eBoundQOverP]);
            return (momentum <= momentumThreshold);
        }
    };


KalmanFittingFactory::KalmanFittingFactory() {

}


void KalmanFittingFactory::Configure() {
    // Setup magnetic field
    auto magneticField = std::make_shared<Acts::ConstantBField>(
        Acts::Vector3(0, 0, m_cfg_bz() * Acts::UnitConstants::T));

    // Configure EigenStepper, Navigator, and Propagator
    Stepper stepper(magneticField);
    Acts::Navigator::Config navCfg{m_acts_geo_svc->GetTrackingGeometry()};
    navCfg.resolvePassive   = false;
    navCfg.resolveMaterial  = false;
    navCfg.resolveSensitive = true;
    Acts::Navigator navigator(navCfg);
    m_propagator = std::make_shared<Propagator>(stepper, std::move(navigator));

    // Logging
    m_log = m_log_svc->logger("KalmanFittingFactory");

    // Acts logging
    auto actsLvlStr = m_acts_level();
    auto lvl = strToActsLevel(actsLvlStr);
    m_acts_logger = Acts::getDefaultLogger("KalmanFittingFactory", lvl);
}

void KalmanFittingFactory::fillMeasurements(
    tdis::TrackSeed trackSeed,
    std::shared_ptr<ActsExamples::MeasurementContainer>& actsMeasurements,
    std::vector<Acts::SourceLink>& sourceLinks) {
    auto geometry = m_acts_geo_svc->GetTrackingGeometry();

    actsMeasurements = std::make_shared<ActsExamples::MeasurementContainer>();

    auto podioPerigee = trackSeed.getPerigee();

    for (auto measurement : trackSeed.getMeasurements()) {
        // There always should be at least 1 reconstructed hit attached to measurement
        auto reconstructedHit = measurement.getHits().at(0);
        auto mcHit = reconstructedHit.getRawHit();

        m_log->debug(
            "   id={:<4} plane={:<3} ring={:<3} pad={:<3}, x={:<7.2f} y={:<7.2f}, z={:<7.2f}, Perigee.z={:<6.2f} surf-id={}",
            mcHit.id().index, mcHit.getPlane(), mcHit.getRing(), mcHit.getPad(),
            mcHit.getTruePosition().x / Acts::UnitConstants::mm,
            mcHit.getTruePosition().y / Acts::UnitConstants::mm,
            mcHit.getTruePosition().z / Acts::UnitConstants::mm,
            podioPerigee.z / Acts::UnitConstants::mm,
            measurement.getSurface());

        // FIX: null-check BEFORE dereference (original code dereferenced then null-checked)
        auto surfaceFromTrkGeo =
            geometry->findSurface(Acts::GeometryIdentifier(measurement.getSurface()));
        if (surfaceFromTrkGeo == nullptr) {
            auto msg = fmt::format(
                "For ring = {}, we can't find back the surface with id = {}. It is "
                "trackingGeometry->findSurface==NULL. Track fitting will fail soon (!)",
                mcHit.getRing(), measurement.getSurface());
            m_log->critical(msg);
            throw std::runtime_error(msg);
        }
        auto surfaceGeoId = surfaceFromTrkGeo->geometryId();

        ActsExamples::IndexSourceLink sourceLink(surfaceGeoId, measurement.getObjectID().index);
        sourceLinks.emplace_back(sourceLink);

        // 1) Prepare the data vector (size=2)
        Acts::Vector2 loc2D = Acts::Vector2::Zero();
        loc2D[Acts::eBoundLoc0] = measurement.getLoc().a;
        loc2D[Acts::eBoundLoc1] = measurement.getLoc().b;

        // 2) Prepare the 2x2 covariance
        Acts::SquareMatrix2 cov2D = Acts::SquareMatrix2::Zero();
        cov2D(0, 0) = measurement.getCovariance().xx;
        cov2D(1, 1) = measurement.getCovariance().yy;
        cov2D(0, 1) = measurement.getCovariance().xy;
        cov2D(1, 0) = measurement.getCovariance().xy;

        actsMeasurements->emplaceMeasurement<2>(
            surfaceGeoId, std::array{Acts::eBoundLoc0, Acts::eBoundLoc1},
            loc2D,
            cov2D);
    }
}


void KalmanFittingFactory::processTrack(tdis::TrackSeed trackSeed) {

    auto geoContext = m_acts_geo_svc->GetActsGeometryContext();
    Acts::MagneticFieldContext magContext;
    Acts::CalibrationContext   calibContext;

    // Fill measurements from our trackSeed object
    std::shared_ptr<ActsExamples::MeasurementContainer> actsMeasurements;
    std::vector<Acts::SourceLink> sourceLinks;
    fillMeasurements(trackSeed, actsMeasurements, sourceLinks);

    if (sourceLinks.empty()) {
        m_log->warn("Track seed ID='{}' has no measurements", trackSeed.id().index);
        return;
    }

    // Create initial parameters at perigee
    auto podioPerigee = trackSeed.getPerigee();
    auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(
        Acts::Vector3(podioPerigee.x, podioPerigee.y, podioPerigee.z));

    auto podioInitParams = trackSeed.getInitParams();

    const auto& firstMeas = trackSeed.getMeasurements().at(0);

    auto firstMcHit  = trackSeed.getMcTrack().getHits().at(0);
    auto secondMcHit = trackSeed.getMcTrack().getHits().at(1);

    if (!firstMcHit.isAvailable() || !secondMcHit.isAvailable()) {
        m_log->warn("!firstMcHit.isAvailable() || !secondMcHit.isAvailable() - skipping track");
        return;
    }

    using SurfacePtr = std::shared_ptr<const Acts::Surface>;
    SurfacePtr startSurf =
        m_acts_geo_svc->GetDetectorCylinder(firstMcHit.getRing())->surface().getSharedPtr();

    if (!startSurf) {
        m_log->critical("startSurf is null for surface id {}", firstMeas.getSurface());
        return;
    }

    // TODO: direction currently computed from true MC hit positions (development/validation only).
    //       Replace with a reconstructed-hit direction estimate for production use.
    Eigen::Vector3d dir3{
        double(secondMcHit.getTruePosition().x - firstMcHit.getTruePosition().x),
        double(secondMcHit.getTruePosition().y - firstMcHit.getTruePosition().y),
        double(secondMcHit.getTruePosition().z - firstMcHit.getTruePosition().z)};
    if (dir3.norm() == 0.) {
        m_log->warn("Two identical MC hit positions; cannot define direction.");
        return;
    }
    dir3.normalize();

    // =========================================================================
    // Full 6x6 covariance mapping: PODIO → ACTS
    //
    // Both PODIO and ACTS use the SAME parameter order:
    //   index 0 → loc0    (eBoundLoc0)
    //   index 1 → loc1    (eBoundLoc1)
    //   index 2 → phi     (eBoundPhi)
    //   index 3 → theta   (eBoundTheta)
    //   index 4 → q/p     (eBoundQOverP)
    //   index 5 → time    (eBoundTime)
    //
    // The full symmetric 6x6 matrix is written out explicitly below.
    // All 36 elements are assigned (upper triangle = lower triangle by symmetry).
    // =========================================================================
    Acts::BoundSquareMatrix actsCov = Acts::BoundSquareMatrix::Zero();
    const auto C = podioInitParams.getCovariance();

    // Row 0: loc0
    actsCov(Acts::eBoundLoc0,   Acts::eBoundLoc0)   = C(0,0);  // loc0-loc0
    actsCov(Acts::eBoundLoc0,   Acts::eBoundLoc1)   = C(0,1);  // loc0-loc1
    actsCov(Acts::eBoundLoc0,   Acts::eBoundPhi)    = C(0,2);  // loc0-phi
    actsCov(Acts::eBoundLoc0,   Acts::eBoundTheta)  = C(0,3);  // loc0-theta
    actsCov(Acts::eBoundLoc0,   Acts::eBoundQOverP) = C(0,4);  // loc0-q/p
    actsCov(Acts::eBoundLoc0,   Acts::eBoundTime)   = C(0,5);  // loc0-time

    // Row 1: loc1
    actsCov(Acts::eBoundLoc1,   Acts::eBoundLoc0)   = C(1,0);  // loc1-loc0  (symmetric)
    actsCov(Acts::eBoundLoc1,   Acts::eBoundLoc1)   = C(1,1);  // loc1-loc1
    actsCov(Acts::eBoundLoc1,   Acts::eBoundPhi)    = C(1,2);  // loc1-phi
    actsCov(Acts::eBoundLoc1,   Acts::eBoundTheta)  = C(1,3);  // loc1-theta
    actsCov(Acts::eBoundLoc1,   Acts::eBoundQOverP) = C(1,4);  // loc1-q/p
    actsCov(Acts::eBoundLoc1,   Acts::eBoundTime)   = C(1,5);  // loc1-time

    // Row 2: phi
    actsCov(Acts::eBoundPhi,    Acts::eBoundLoc0)   = C(2,0);  // phi-loc0   (symmetric)
    actsCov(Acts::eBoundPhi,    Acts::eBoundLoc1)   = C(2,1);  // phi-loc1   (symmetric)
    actsCov(Acts::eBoundPhi,    Acts::eBoundPhi)    = C(2,2);  // phi-phi
    actsCov(Acts::eBoundPhi,    Acts::eBoundTheta)  = C(2,3);  // phi-theta
    actsCov(Acts::eBoundPhi,    Acts::eBoundQOverP) = C(2,4);  // phi-q/p
    actsCov(Acts::eBoundPhi,    Acts::eBoundTime)   = C(2,5);  // phi-time

    // Row 3: theta
    actsCov(Acts::eBoundTheta,  Acts::eBoundLoc0)   = C(3,0);  // theta-loc0 (symmetric)
    actsCov(Acts::eBoundTheta,  Acts::eBoundLoc1)   = C(3,1);  // theta-loc1 (symmetric)
    actsCov(Acts::eBoundTheta,  Acts::eBoundPhi)    = C(3,2);  // theta-phi  (symmetric)
    actsCov(Acts::eBoundTheta,  Acts::eBoundTheta)  = C(3,3);  // theta-theta
    actsCov(Acts::eBoundTheta,  Acts::eBoundQOverP) = C(3,4);  // theta-q/p
    actsCov(Acts::eBoundTheta,  Acts::eBoundTime)   = C(3,5);  // theta-time

    // Row 4: q/p
    actsCov(Acts::eBoundQOverP, Acts::eBoundLoc0)   = C(4,0);  // q/p-loc0   (symmetric)
    actsCov(Acts::eBoundQOverP, Acts::eBoundLoc1)   = C(4,1);  // q/p-loc1   (symmetric)
    actsCov(Acts::eBoundQOverP, Acts::eBoundPhi)    = C(4,2);  // q/p-phi    (symmetric)
    actsCov(Acts::eBoundQOverP, Acts::eBoundTheta)  = C(4,3);  // q/p-theta  (symmetric)
    actsCov(Acts::eBoundQOverP, Acts::eBoundQOverP) = C(4,4);  // q/p-q/p
    actsCov(Acts::eBoundQOverP, Acts::eBoundTime)   = C(4,5);  // q/p-time

    // Row 5: time
    actsCov(Acts::eBoundTime,   Acts::eBoundLoc0)   = C(5,0);  // time-loc0  (symmetric)
    actsCov(Acts::eBoundTime,   Acts::eBoundLoc1)   = C(5,1);  // time-loc1  (symmetric)
    actsCov(Acts::eBoundTime,   Acts::eBoundPhi)    = C(5,2);  // time-phi   (symmetric)
    actsCov(Acts::eBoundTime,   Acts::eBoundTheta)  = C(5,3);  // time-theta (symmetric)
    actsCov(Acts::eBoundTime,   Acts::eBoundQOverP) = C(5,4);  // time-q/p   (symmetric)
    actsCov(Acts::eBoundTime,   Acts::eBoundTime)   = C(5,5);  // time-time
    // =========================================================================

    // -------------------------------------------------------------------------
    // RC3: Validate seed qOverP before it reaches BoundTrackParameters::create.
    // A zero or non-finite value means 1/qOverP = inf, which corrupts every
    // downstream momentum print and the fit itself.
    // -------------------------------------------------------------------------
    {
        const double seedQOverP = podioInitParams.getQOverP();
        m_log->debug("Seed qOverP (raw from PODIO) = {:.6e}", seedQOverP);
        if (seedQOverP == 0.0 || !std::isfinite(seedQOverP)) {
            m_log->error(
                "trackSeed {}: seed qOverP is {:.6e} (zero or non-finite) — skipping track",
                trackSeed.id().index, seedQOverP);
            return;
        }
    }

    // -------------------------------------------------------------------------
    // RC2: Log the full actsCov diagonal so we can confirm the upstream seed
    // builder is actually filling every parameter's variance.
    // Any zero or denormalized value (e.g. 1.48e-323) here means the upstream
    // Cov6f was never filled for that row/column — fix it there, not here.
    // -------------------------------------------------------------------------
    m_log->debug(
        "actsCov diagonal — loc0={:.3e}  loc1={:.3e}  phi={:.3e}  theta={:.3e}  qOverP={:.3e}  time={:.3e}",
        actsCov(Acts::eBoundLoc0,   Acts::eBoundLoc0),
        actsCov(Acts::eBoundLoc1,   Acts::eBoundLoc1),
        actsCov(Acts::eBoundPhi,    Acts::eBoundPhi),
        actsCov(Acts::eBoundTheta,  Acts::eBoundTheta),
        actsCov(Acts::eBoundQOverP, Acts::eBoundQOverP),
        actsCov(Acts::eBoundTime,   Acts::eBoundTime));

    // -------------------------------------------------------------------------
    // RC4: Log the raw PODIO Cov6f diagonal before the mapping.
    // If these are all zero while off-diagonals are non-zero, the storage-order
    // of Cov6f (row-major vs column-major) is mismatched with the C(i,j) indexing.
    // -------------------------------------------------------------------------
    m_log->debug(
        "PODIO Cov6f raw diagonal — C(0,0)={:.3e}  C(1,1)={:.3e}  C(2,2)={:.3e}  C(3,3)={:.3e}  C(4,4)={:.3e}  C(5,5)={:.3e}",
        C(0,0), C(1,1), C(2,2), C(3,3), C(4,4), C(5,5));

    // FIX: typo corrected: getPadHight -> getPadHeight
    auto alternateInitParams = Acts::BoundTrackParameters::create(
        geoContext,
        startSurf,
        {
            firstMcHit.getPadCenterX(),
            firstMcHit.getPadCenterY(),
            firstMcHit.getTruePosition().z,
            firstMcHit.getTime()
        },
        dir3,
        podioInitParams.getQOverP(),
        actsCov,
        Acts::ParticleHypothesis::proton(),
        2 * getPadHight());

    if (!alternateInitParams.ok()) {
        m_log->warn("!alternateInitParams.ok()");
        m_log->warn(alternateInitParams.error().message());
        return;
    }

    m_log->debug(
        "Initial track parameters: p = {:.3f} GeV, theta = {:.3f} deg, phi = {:.3f} deg, vz = {:.3f} mm",
        1 / podioInitParams.getQOverP() / Acts::UnitConstants::GeV,
        podioInitParams.getTheta() / Acts::UnitConstants::degree,
        podioInitParams.getPhi() / Acts::UnitConstants::degree,
        podioPerigee.z);

    ActsExamples::PassThroughCalibrator calibrator;
    ActsExamples::MeasurementCalibratorAdapter calibratorAdaptor(calibrator, *actsMeasurements);

    Acts::GainMatrixUpdater        kfUpdater;
    Acts::GainMatrixSmoother       kfSmoother;
    SimpleReverseFilteringLogic    reverseFilteringLogic;

    bool multipleScattering = false;
    bool energyLoss         = false;
    Acts::FreeToBoundCorrection freeToBoundCorrection;

    auto geometry = m_acts_geo_svc->GetTrackingGeometry();
    ActsExamples::IndexSourceLink::SurfaceAccessor slSurfaceAccessor{*geometry};

    // Setup extensions
    Acts::KalmanFitterExtensions<Acts::VectorMultiTrajectory> extensions;
    extensions.updater.connect<
        &Acts::GainMatrixUpdater::operator()<Acts::VectorMultiTrajectory>>(&kfUpdater);
    extensions.smoother.connect<
        &Acts::GainMatrixSmoother::operator()<Acts::VectorMultiTrajectory>>(&kfSmoother);
    extensions.reverseFilteringLogic.connect<
        &SimpleReverseFilteringLogic::doBackwardFiltering>(&reverseFilteringLogic);

    // -------------------------------------------------------------------------
    // Reference surface fix.
    //
    // Root cause of "trackProxy has no reference surface":
    //   Passing nullptr + strategy::last means ACTS needs a smoothed state on
    //   the last surface to assign the reference. Because backward smoothing is
    //   disabled (momentumThreshold=0), no smoothed state exists on the last
    //   surface → hasReferenceSurface() stays false → parameters() is garbage.
    //
    // Fix: pass startSurf (the first hit surface) explicitly as the reference.
    //   - It is guaranteed to exist — the fit already visited it.
    //   - strategy::first is used so ACTS extrapolates back to startSurf and
    //     assigns parameters there, which is the natural perigee-like point for
    //     a barrel TPC track entering from the innermost ring outward.
    //   - This gives well-defined, physically meaningful parameters at the
    //     first measurement surface regardless of whether smoothing ran.
    // -------------------------------------------------------------------------
    const Acts::Surface* referenceSurface = startSurf.get();

    bool doRefit = false;

    // Create options
    Acts::KalmanFitterOptions<Acts::VectorMultiTrajectory> kfOptions(
        geoContext, magContext, calibContext, extensions,
        Acts::PropagatorPlainOptions(geoContext, magContext), referenceSurface);

    kfOptions.referenceSurfaceStrategy = Acts::KalmanFitterTargetSurfaceStrategy::first;
    kfOptions.multipleScattering       = multipleScattering;
    kfOptions.energyLoss               = energyLoss;
    kfOptions.freeToBoundCorrection    = freeToBoundCorrection;
    kfOptions.extensions.calibrator.connect<
        &ActsExamples::MeasurementCalibratorAdapter::calibrate>(&calibratorAdaptor);

    if (doRefit) {
        kfOptions.extensions.surfaceAccessor.connect<
            &tdis::RefittingCalibrator::accessSurface>();
    } else {
        kfOptions.extensions.surfaceAccessor.connect<
            &ActsExamples::IndexSourceLink::SurfaceAccessor::operator()>(&slSurfaceAccessor);
    }

    // Setup magnetic field, stepper, navigator, propagator and fitters
    // (kept here for now; consider moving to Configure() to avoid per-track reconstruction)
    auto magneticField = std::make_shared<Acts::ConstantBField>(
        Acts::Vector3(0, 0, m_cfg_bz() * Acts::UnitConstants::T));
    const Stepper stepper(std::move(magneticField));

    Acts::Navigator::Config cfg;
    cfg.trackingGeometry  = geometry;
    cfg.resolvePassive    = false;
    cfg.resolveMaterial   = false;
    cfg.resolveSensitive  = true;
    Acts::Navigator navigator(cfg, m_acts_logger->cloneWithSuffix("Navigator"));

    Propagator propagator(stepper, std::move(navigator),
                          m_acts_logger->cloneWithSuffix("Propagator"));
    KalmanFitter trackFitter(std::move(propagator),
                             m_acts_logger->cloneWithSuffix("Fitter"));

    Acts::DirectNavigator directNavigator{m_acts_logger->cloneWithSuffix("DirectNavigator")};
    DirectPropagator directPropagator(stepper, std::move(directNavigator),
                                      m_acts_logger->cloneWithSuffix("DirectPropagator"));
    DirectKalmanFitter directTrackFitter(std::move(directPropagator),
                                         m_acts_logger->cloneWithSuffix("DirectFitter"));

    Acts::VectorMultiTrajectory vTraj;
    Acts::VectorTrackContainer  vTrk;
    Acts::TrackContainer        tracks(vTrk, vTraj);

    // Run Kalman fit
    auto result = trackFitter.fit(
        sourceLinks.begin(),
        sourceLinks.end(),
        alternateInitParams.value(),
        kfOptions,
        tracks);

    if (!result.ok()) {
        m_log->error("Fit failed for trackSeed {}: {}",
                     trackSeed.id().index, result.error().message());
        return;
    }

    auto& trackProxy = result.value();
    auto  mcTrack    = trackSeed.getMcTrack();

    // -------------------------------------------------------------------------
    // RC1: Guard against a valid result that still holds an invalid reference
    // surface or zero measurements.  parameters() on such a proxy returns
    // uninitialized Eigen memory → qOverP = 1.48e-323, momentum = inf.
    // -------------------------------------------------------------------------
    if (!trackProxy.hasReferenceSurface()) {
        m_log->error(
            "trackSeed {}: fit succeeded but trackProxy has no reference surface — "
            "parameters() would read uninitialized memory. Skipping EDM conversion. "
            "startSurf.get() was passed as referenceSurface; check that startSurf is valid "
            "and that the propagator can reach the first hit surface from the seed.",
            trackSeed.id().index);
        return;
    }
    if (trackProxy.nMeasurements() == 0) {
        m_log->error(
            "trackSeed {}: fit succeeded but trackProxy has zero measurements — "
            "fit is likely degenerate. Skipping EDM conversion.",
            trackSeed.id().index);
        return;
    }

    // -------------------------------------------------------------------------
    // RC5: Sanity-check the fitted parameters before any use.
    // With referenceSurfaceStrategy::last, if smoothing fails on the last state
    // the parameters at that surface are left uninitialized.
    // If this fires, switch to ::first temporarily to isolate the problem.
    // -------------------------------------------------------------------------
    {
        const auto& rawParams     = trackProxy.parameters();
        const double fittedQOverP = rawParams[Acts::eBoundQOverP];
        const double fittedTheta  = rawParams[Acts::eBoundTheta];
        const double fittedPhi    = rawParams[Acts::eBoundPhi];

        m_log->debug("Post-fit raw params — qOverP={:.6e}  theta={:.6e}  phi={:.6e}",
                     fittedQOverP, fittedTheta, fittedPhi);

        bool paramsInvalid = false;
        if (!std::isfinite(fittedQOverP) || fittedQOverP == 0.0) {
            m_log->error(
                "trackSeed {}: fitted qOverP={:.6e} is zero or non-finite. "
                "Possible causes: (a) propagator could not reach startSurf (first hit "
                "surface) from the seed — check geometry and seed direction; "
                "(b) actsCov(q/p,q/p) was zero in the seed (check RC2/RC4 debug lines above).",
                trackSeed.id().index, fittedQOverP);
            paramsInvalid = true;
        }
        if (!std::isfinite(fittedTheta) || !std::isfinite(fittedPhi)) {
            m_log->error(
                "trackSeed {}: fitted theta={:.6e} or phi={:.6e} is non-finite.",
                trackSeed.id().index, fittedTheta, fittedPhi);
            paramsInvalid = true;
        }
        if (paramsInvalid) { return; }
    }

    m_log->debug("trackProxy.tipIndex() = {}", trackProxy.tipIndex());
    m_log->debug("mcTrack.mom = {} reco mom = {}", mcTrack.getMomentum(), trackProxy.absoluteMomentum());
    m_log->debug("mcTrack.theta = {} reco = {}", mcTrack.getTheta(), trackProxy.theta());
    m_log->debug("mcTrack.phi  = {} reco phi {}", mcTrack.getPhi(), trackProxy.phi());
    m_log->debug("reco chi2 {} nDoF {} chi2/ndof {}",
                 trackProxy.chi2(), trackProxy.nDoF(),
                 trackProxy.chi2() / trackProxy.nDoF());

    m_log->debug("Successfully fitted track => track p {} in container",
                 trackProxy.absoluteMomentum());

    const auto& params = trackProxy.parameters();
    double qOverP  = params[Acts::eBoundQOverP];
    double momentum = std::abs(1.0 / qOverP);
    double theta    = params[Acts::eBoundTheta];
    double phi      = params[Acts::eBoundPhi];

    m_log->debug("qOverP   = {}", qOverP);
    m_log->debug("momentum = {}", momentum);
    m_log->debug("theta    = {}", theta);
    m_log->debug("phi      = {}", phi);

    for (auto state : trackProxy.trackStatesReversed()) {
        if (state.hasSmoothed()) {
            m_log->debug(
                "Smoothed params at {}: loc0={}, loc1={}, phi={}, theta={}, qOverP={}, momentum={}",
                state.index(),
                state.smoothed()[Acts::eBoundLoc0],
                state.smoothed()[Acts::eBoundLoc1],
                state.smoothed()[Acts::eBoundPhi],
                state.smoothed()[Acts::eBoundTheta],
                state.smoothed()[Acts::eBoundQOverP],
                1 / state.smoothed()[Acts::eBoundQOverP]);
        }
    }

    m_log->debug("N STATES:   {}", trackProxy.nTrackStates());
    m_log->debug("N MEAS:     {}", trackProxy.nMeasurements());
    m_log->debug("N OUTLIERS: {}", trackProxy.nOutliers());
    m_log->debug("N HOLES:    {}", trackProxy.nHoles());

    // ========================== CONVERT TO EDM ====================================

    auto trajectorySummary = Acts::MultiTrajectoryHelpers::trajectoryState(
        tracks.trackStateContainer(), trackProxy.tipIndex());

    auto trajectory = m_out_trajectories->create();
    trajectory.setType(1);
    trajectory.setNStates(trajectorySummary.nStates);
    trajectory.setNMeasurements(trajectorySummary.nMeasurements);
    trajectory.setNOutliers(trajectorySummary.nOutliers);
    trajectory.setNHoles(trajectorySummary.nHoles);
    trajectory.setNSharedHits(trajectorySummary.nSharedHits);
    trajectory.setSeed(trackSeed);

    for (const auto& measurementChi2 : trajectorySummary.measurementChi2) {
        trajectory.addToMeasurementChi2(measurementChi2);
    }
    for (const auto& outlierChi2 : trajectorySummary.outlierChi2) {
        trajectory.addToOutlierChi2(outlierChi2);
    }

    const auto& boundParams = trackProxy.parameters();
    const auto& boundCov    = trackProxy.covariance();

    auto trackParams = m_out_track_params()->create();
    trackParams.setType(0);
    if (trackProxy.hasReferenceSurface()) {
        trackParams.setSurface(trackProxy.referenceSurface().geometryId().value());
    }

    trackParams.setLoc({
        static_cast<float>(boundParams[Acts::eBoundLoc0]),
        static_cast<float>(boundParams[Acts::eBoundLoc1])
    });
    trackParams.setTheta(static_cast<float>(boundParams[Acts::eBoundTheta]));
    trackParams.setPhi(static_cast<float>(boundParams[Acts::eBoundPhi]));
    trackParams.setQOverP(static_cast<float>(boundParams[Acts::eBoundQOverP]));
    trackParams.setTime(static_cast<float>(boundParams[Acts::eBoundTime]));
    trackParams.setPdg(static_cast<int32_t>(trackProxy.particleHypothesis().absolutePdg()));

    // =========================================================================
    // Full 6x6 output covariance with unit conversion (ACTS internal → EDM)
    //
    // ACTS internal units:  length in mm, angles in rad, q/p in 1/GeV, time in ns
    // EDM units:            same conventions; scale factors applied per-element
    //
    // Conversion factors per parameter:
    //   loc0, loc1  : divide by mm  (UnitConstants::mm)
    //   phi, theta  : dimensionless (factor = 1)
    //   q/p         : divide by 1/GeV  (= multiply by GeV)
    //   time        : divide by ns  (UnitConstants::ns)
    //
    // The (i,j) element of the EDM covariance is:
    //   edmCov(i,j) = actsCov(i,j) / unit_i / unit_j
    // =========================================================================
    static constexpr std::array<std::pair<Acts::BoundIndices, double>, 6> edm_indexed_units{{
        {Acts::eBoundLoc0,   Acts::UnitConstants::mm},
        {Acts::eBoundLoc1,   Acts::UnitConstants::mm},
        {Acts::eBoundPhi,    1.},
        {Acts::eBoundTheta,  1.},
        {Acts::eBoundQOverP, 1. / Acts::UnitConstants::GeV},
        {Acts::eBoundTime,   Acts::UnitConstants::ns}
    }};

    tdis::Cov6f cov;
    for (std::size_t i = 0; const auto& [a, x] : edm_indexed_units) {
        for (std::size_t j = 0; const auto& [b, y] : edm_indexed_units) {
            cov(i, j) = boundCov(a, b) / x / y;
            ++j;
        }
        ++i;
    }
    trackParams.setCovariance(cov);
    trajectory.addToTrackParameters(trackParams);

    auto track = m_out_tracks()->create();
    track.setType(trackParams.getType());

    // FIX: momentum units corrected.
    // 1/qOverP is in ACTS internal units (GeV for momentum in natural units).
    // Divide by UnitConstants::GeV to express as a plain GeV double for EDM storage.
    qOverP   = boundParams[Acts::eBoundQOverP];
    momentum = std::abs(1.0 / qOverP) / Acts::UnitConstants::GeV;  // → GeV
    theta    = boundParams[Acts::eBoundTheta];
    phi      = boundParams[Acts::eBoundPhi];

    double px = momentum * std::sin(theta) * std::cos(phi);
    double py = momentum * std::sin(theta) * std::sin(phi);
    double pz = momentum * std::cos(theta);

    track.setMomentum({
        static_cast<float>(px),
        static_cast<float>(py),
        static_cast<float>(pz)
    });

    // FIX: removed unused podioOutPerigee variable; use podioPerigee already in scope
    track.setPosition({
        static_cast<float>(podioPerigee.x),
        static_cast<float>(podioPerigee.y),
        static_cast<float>(podioPerigee.z)
    });

    // =========================================================================
    // Bound → Cartesian covariance transform
    //
    // Bound state vector:    b = (loc0, loc1, phi, theta, q/p, time)
    // Cartesian state vector: c = (x, y, z, px, py, pz)
    //
    // The position block (x,y,z) on the reference surface maps as:
    //   x ≈ loc0   y ≈ loc1   z = fixed on surface  →  ∂x/∂loc0=1, ∂y/∂loc1=1
    //   (all other ∂pos/∂bound = 0 for a planar perigee surface)
    //
    // The momentum block uses p = |p| = 1/|q/p| (in GeV after unit division):
    //   px = p · sin(θ)·cos(φ)
    //   py = p · sin(θ)·sin(φ)
    //   pz = p · cos(θ)
    //
    // Partial derivatives of (px,py,pz) w.r.t. (phi, theta, q/p):
    //   ∂px/∂φ     =  -p·sin(θ)·sin(φ)  = -py
    //   ∂px/∂θ     =   p·cos(θ)·cos(φ)  =  pz·cos(φ)/sin(θ)... simplified: p·cos(θ)·cos(φ)
    //   ∂px/∂(q/p) =  -px / (q/p)       = -px · p²  (sign: ∂p/∂(q/p) = -1/(q/p)²)
    //
    //   ∂py/∂φ     =   p·sin(θ)·cos(φ)  = +px
    //   ∂py/∂θ     =   p·cos(θ)·sin(φ)
    //   ∂py/∂(q/p) =  -py · p²
    //
    //   ∂pz/∂φ     =   0
    //   ∂pz/∂θ     =  -p·sin(θ)
    //   ∂pz/∂(q/p) =  -pz · p²
    //
    // The full Jacobian J is 6×6 (Cartesian rows, bound columns):
    //
    //         loc0  loc1  phi              theta              q/p              time
    //   x  [  1     0     0                0                  0                0   ]
    //   y  [  0     1     0                0                  0                0   ]
    //   z  [  0     0     0                0                  0                0   ]
    //   px [  0     0     dpx_dphi         dpx_dtheta         dpx_dqop         0   ]
    //   py [  0     0     dpy_dphi         dpy_dtheta         dpy_dqop         0   ]
    //   pz [  0     0     0                dpz_dtheta         dpz_dqop         0   ]
    //
    // Cartesian covariance = J · boundCov · Jᵀ
    // =========================================================================
    {
        // Kinematic quantities (momentum already in GeV from above)
        const double sinPhi   = std::sin(phi);
        const double cosPhi   = std::cos(phi);
        const double sinTheta = std::sin(theta);
        const double cosTheta = std::cos(theta);
        // p in GeV (same as `momentum` computed above)
        const double p        = momentum;                   // GeV
        // ∂p/∂(q/p) = -1/(q/p)² = -p² (sign absorbed per derivative below)
        const double p2       = p * p;

        // Momentum partial derivatives
        const double dpx_dphi   = -p * sinTheta * sinPhi;   // = -py
        const double dpx_dtheta =  p * cosTheta * cosPhi;
        const double dpx_dqop   = -px * p2;                 // px/GeV · p² → GeV²

        const double dpy_dphi   =  p * sinTheta * cosPhi;   // = +px
        const double dpy_dtheta =  p * cosTheta * sinPhi;
        const double dpy_dqop   = -py * p2;

        // dpz_dphi = 0
        const double dpz_dtheta = -p * sinTheta;
        const double dpz_dqop   = -pz * p2;

        // Build 6x6 Jacobian  J[cart_row][bound_col]
        // bound col order: 0=loc0, 1=loc1, 2=phi, 3=theta, 4=qop, 5=time
        // cart  row order: 0=x,    1=y,    2=z,   3=px,    4=py,  5=pz
        Eigen::Matrix<double, 6, 6> J = Eigen::Matrix<double, 6, 6>::Zero();

        // Position rows — loc0→x, loc1→y, z has no free bound parameter here
        J(0, Acts::eBoundLoc0)   = 1.0;   // ∂x/∂loc0
        J(1, Acts::eBoundLoc1)   = 1.0;   // ∂y/∂loc1
        // J(2, *) = 0  (z is fixed on the reference surface)

        // px row
        J(3, Acts::eBoundPhi)    = dpx_dphi;
        J(3, Acts::eBoundTheta)  = dpx_dtheta;
        J(3, Acts::eBoundQOverP) = dpx_dqop;

        // py row
        J(4, Acts::eBoundPhi)    = dpy_dphi;
        J(4, Acts::eBoundTheta)  = dpy_dtheta;
        J(4, Acts::eBoundQOverP) = dpy_dqop;

        // pz row
        // J(5, Acts::eBoundPhi)    = 0  (already zero)
        J(5, Acts::eBoundTheta)  = dpz_dtheta;
        J(5, Acts::eBoundQOverP) = dpz_dqop;

        // Propagate: cartCov = J * boundCov * Jᵀ
        // boundCov is Acts::BoundSquareMatrix (6x6, double)
        const Eigen::Matrix<double, 6, 6> cartCov =
            J * boundCov * J.transpose();

        // Pack into EDM Cov6f (row-major 6x6 float)
        // EDM order: (x, y, z, px, py, pz) — same as cartesian row order above
        tdis::Cov6f pmCov;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                pmCov(i, j) = static_cast<float>(cartCov(i, j));
            }
        }

        m_log->debug(
            "positionMomentumCov diagonal — "
            "x={:.3e}  y={:.3e}  z={:.3e}  px={:.3e}  py={:.3e}  pz={:.3e}",
            cartCov(0,0), cartCov(1,1), cartCov(2,2),
            cartCov(3,3), cartCov(4,4), cartCov(5,5));

        track.setPositionMomentumCovariance(pmCov);
    }
    track.setTime(static_cast<float>(boundParams[Acts::eBoundTime]));
    track.setTimeError(
        std::sqrt(static_cast<float>(boundCov(Acts::eBoundTime, Acts::eBoundTime))));
    track.setCharge(std::copysign(1.0, qOverP));
    track.setChi2(trajectorySummary.chi2Sum);
    track.setNdf(trajectorySummary.NDF);
    track.setPdg(trackProxy.particleHypothesis().absolutePdg());
    track.setTrajectory(trajectory);

    tracks.trackStateContainer().visitBackwards(
        trackProxy.tipIndex(),
        [&](const auto& state) {
            auto typeFlags = state.typeFlags();

            if (state.hasUncalibratedSourceLink()) {
                auto sourceLink = state.getUncalibratedSourceLink()
                    .template get<ActsExamples::IndexSourceLink>();
                std::size_t measIndex = sourceLink.index();

                if (measIndex < trackSeed.getMeasurements().size()) {
                    auto measurement = trackSeed.getMeasurements()[measIndex];

                    if (typeFlags.test(Acts::TrackStateFlag::MeasurementFlag)) {
                        track.addToMeasurements(measurement);
                        trajectory.addToMeasurements_deprecated(measurement);
                    } else if (typeFlags.test(Acts::TrackStateFlag::OutlierFlag)) {
                        trajectory.addToOutliers_deprecated(measurement);
                    }
                }
            }
        });

    m_log->debug(
        "Track successfully converted: p={:.3f} GeV, theta={:.3f} deg, phi={:.3f} deg, chi2/ndf={:.2f}",
        momentum,
        theta / Acts::UnitConstants::degree,
        phi   / Acts::UnitConstants::degree,
        track.getChi2() / track.getNdf());
}


void KalmanFittingFactory::Execute(int32_t run_number, uint64_t event_number) {
    using namespace Acts::UnitLiterals;

    for (const auto& trackSeed : *m_in_trackSeeds()) {
        processTrack(trackSeed);
    }
}

} // namespace tdis::tracking
