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
#include "LayerCandidateCalibrator.h"
#include "FittedTrackOutput.h"

#include <set>
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
    if (!std::isfinite(m_cfg_bz())) {
        throw std::invalid_argument("KalmanFittingFactory:bz must be finite [T]");
    }
    if (!std::isfinite(m_cfg_chi2Cut()) || m_cfg_chi2Cut() < 0) {
        throw std::invalid_argument("KalmanFittingFactory:chi2_cut must be finite and nonnegative");
    }
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
    m_log->info("Using Bz = {} T", m_cfg_bz());
    m_log->info("Pre-update innovation chi2 cut = {} (0 disables)", m_cfg_chi2Cut());

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

    std::set<Acts::GeometryIdentifier> linkedSurfaces;
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

        // Indices refer to this seed's ACTS measurement container, not global EDM IDs.
        ActsExamples::IndexSourceLink sourceLink(surfaceGeoId, actsMeasurements->size());
        // KalmanFitter takes one link per surface. The calibrator evaluates all
        // candidates on that surface and replaces this representative link.
        if (linkedSurfaces.insert(surfaceGeoId).second) {
            sourceLinks.emplace_back(sourceLink);
        }

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


void KalmanFittingFactory::processTrack(tdis::TrackSeed trackSeed, int32_t runNumber, uint64_t eventNumber) {

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
        Acts::Vector3::Zero());

    auto podioInitParams = trackSeed.getInitParams();

    // Seeds originate near the beam axis. Start navigation at the innermost
    // candidate cylinder, independently of input time ordering or repeated pads.
    const Acts::Surface* startSurf = nullptr;
    double firstRadius = std::numeric_limits<double>::infinity();
    for (const auto& measurement : trackSeed.getMeasurements()) {
        const auto* surface = m_acts_geo_svc->GetTrackingGeometry()->findSurface(
            Acts::GeometryIdentifier(measurement.getSurface()));
        const auto& bounds = dynamic_cast<const Acts::CylinderBounds&>(surface->bounds());
        const double radius = bounds.get(Acts::CylinderBounds::eR);
        if (radius < firstRadius) {
            firstRadius = radius;
            startSurf = surface;
        }
    }

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

    // EDM time is ns; ACTS time is c*t. Scale each covariance index separately,
    // including correlations (the time variance receives two factors of ns).
    actsCov.row(Acts::eBoundTime) *= Acts::UnitConstants::ns;
    actsCov.col(Acts::eBoundTime) *= Acts::UnitConstants::ns;
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

    Acts::BoundVector seedParameters;
    seedParameters << podioInitParams.getLoc().a, podioInitParams.getLoc().b,
        podioInitParams.getPhi(), podioInitParams.getTheta(),
        podioInitParams.getQOverP(), podioInitParams.getTime() * Acts::UnitConstants::ns;
    if (!seedParameters.allFinite() || !actsCov.allFinite()) {
        m_log->warn("Non-finite seed parameters or covariance; skipping track {}", trackSeed.id().index);
        return;
    }
    const Acts::BoundTrackParameters initialParameters(
        perigee, seedParameters, actsCov, Acts::ParticleHypothesis::proton());

    // The central region is outside the tracking volume. Propagate explicitly
    // to the first cylinder without geometry navigation, including covariance
    // transport; then let the normal navigator visit the detector layers.
    using SeedPropagator = Acts::Propagator<Stepper>;
    auto seedField = std::make_shared<Acts::ConstantBField>(
        Acts::Vector3(0, 0, m_cfg_bz() * Acts::UnitConstants::T));
    SeedPropagator seedPropagator{Stepper(seedField)};
    SeedPropagator::Options<> seedOptions(geoContext, magContext);
    auto propagatedSeed = seedPropagator.propagate(initialParameters, *startSurf, seedOptions);
    if (!propagatedSeed.ok() || !propagatedSeed.value().endParameters.has_value()) {
        m_log->warn("Cannot propagate seed {} to the innermost candidate cylinder", trackSeed.id().index);
        return;
    }

    m_log->debug(
        "Initial track parameters: p = {:.3f} GeV, theta = {:.3f} rad, phi = {:.3f} rad, vz = {:.3f} mm",
        1 / podioInitParams.getQOverP() / Acts::UnitConstants::GeV,
        podioInitParams.getTheta(),
        podioInitParams.getPhi(),
        podioPerigee.z);

    LayerCandidateCalibrator calibrator(*actsMeasurements);
    InnovationOutlierFinder outlierFinder(m_cfg_chi2Cut());

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
    extensions.outlierFinder.connect<&InnovationOutlierFinder::operator()>(&outlierFinder);

    // Export the fit at an accepted measurement surface. ACTS propagates the
    // perigee seed and its covariance to the cylinders before candidate selection.
    const Acts::Surface* referenceSurface = startSurf;

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
        &LayerCandidateCalibrator::calibrate>(&calibrator);

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
        *propagatedSeed.value().endParameters,
        kfOptions,
        tracks);

    if (!result.ok()) {
        m_log->error("Fit failed for trackSeed {}: {}",
                     trackSeed.id().index, result.error().message());
        return;
    }

    m_log->info("Track seed {}: evaluated {} candidate measurements on {} selected cylinders",
                trackSeed.id().index, actsMeasurements->size(), calibrator.selectedCandidates().size());
    for (const auto& [surface, index] : calibrator.selectedCandidates()) {
        m_log->debug("Selected surface {} candidate {} (EDM measurement {})",
                     surface.value(), index, trackSeed.getMeasurements()[index].getObjectID().index);
    }

    auto& trackProxy = result.value();
    // ACTS's vector track container defaults to a pion hypothesis. Preserve the
    // hypothesis actually used for fitting and for the inward time transport.
    trackProxy.setParticleHypothesis(initialParameters.particleHypothesis());
    auto  mcTrack    = trackSeed.getMcTrack();

    // ACTS can return accepted filter states without a reference when geometry
    // navigation ends before its finalization actor runs after a last-layer
    // outlier. Recover only when all requested cylinders were visited: an
    // incomplete navigation must remain a failure. Never substitute the seed.
    if (!trackProxy.hasReferenceSurface() && trackProxy.nMeasurements() > 0 &&
        calibrator.selectedCandidates().size() == sourceLinks.size()) {
        auto smoothed = kfSmoother(geoContext, vTraj, trackProxy.tipIndex(), *m_acts_logger);
        if (smoothed.ok()) {
            for (auto state : trackProxy.trackStatesReversed()) {
                if (state.typeFlags().test(Acts::TrackStateFlag::MeasurementFlag) &&
                    !state.typeFlags().test(Acts::TrackStateFlag::OutlierFlag) && state.hasSmoothed()) {
                    trackProxy.parameters() = state.smoothed();
                    trackProxy.covariance() = state.smoothedCovariance();
                    trackProxy.setReferenceSurface(state.referenceSurface().getSharedPtr());
                }
            }
            m_log->info("KF reference recovered from smoothed measurements: event={} seed={}",
                        eventNumber, trackSeed.id().index);
        }
    }
    if (!trackProxy.hasReferenceSurface()) {
        m_log->warn("KF has no fitted reference: event={} seed={} visited_cylinders={} requested_cylinders={}",
                    eventNumber, trackSeed.id().index,
                    calibrator.selectedCandidates().size(), sourceLinks.size());
    }

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
    m_log->debug("mcTrack.theta [rad] = {} reco [rad] = {}", mcTrack.getTheta(), trackProxy.theta());
    m_log->debug("mcTrack.phi [rad] = {} reco [rad] = {}", mcTrack.getPhi(), trackProxy.phi());
    m_log->debug("reco chi2 {} nDoF {} chi2/ndof {}",
                 trackProxy.chi2(), trackProxy.nDoF(),
                 trackProxy.chi2() / trackProxy.nDoF());

    m_log->debug("Successfully fitted track => track p {} in container",
                 trackProxy.absoluteMomentum());

    // The KF reference at the first cylinder includes the backward smoothing.
    // Keep it even when the separate inward extrapolation cannot reach perigee.
    const Acts::BoundTrackParameters cylinderParameters(
        trackProxy.referenceSurface().getSharedPtr(), trackProxy.parameters(),
        trackProxy.covariance(), trackProxy.particleHypothesis());
    if (!cylinderParameters.parameters().allFinite() ||
        !cylinderParameters.covariance()->allFinite()) {
        m_log->error("Non-finite fitted state/covariance for seed {}", trackSeed.id().index);
        return;
    }
    auto outputParameters = cylinderParameters;
    auto perigeeResult = extrapolateToPerigee(
        cylinderParameters, seedPropagator, geoContext, magContext);
    const bool perigeeReached = perigeeResult.ok();
    if (perigeeReached) {
        outputParameters = std::move(perigeeResult.value());
        m_log->info("Track seed {}: fitted perigee extrapolation succeeded", trackSeed.id().index);
    } else {
        m_log->warn("Track seed {}: perigee extrapolation failed ({}); retaining cylinder output",
                    trackSeed.id().index, perigeeResult.error().message());
    }

    // Optional portable state points for the event display. Smooth a copy:
    // hasSmoothed() only proves storage exists, and diagnostics must not change
    // the fitted result. An explicit successful smoothing initializes the copy.
    if (m_cfg_diagnosticEvent() == -2 || (m_cfg_diagnosticEvent() >= 0 &&
        eventNumber == static_cast<uint64_t>(m_cfg_diagnosticEvent()))) {
        auto diagnosticStates = vTraj;
        bool validStates = true;
        diagnosticStates.visitBackwards(trackProxy.tipIndex(), [&](const auto& state) {
            validStates = validStates && state.hasPredicted() && state.hasFiltered() &&
                state.predicted().allFinite() && state.filtered().allFinite() &&
                state.predictedCovariance().allFinite() && state.filteredCovariance().allFinite();
        });
        const bool smoothed = validStates &&
            kfSmoother(geoContext, diagnosticStates, trackProxy.tipIndex(), *m_acts_logger).ok();
        diagnosticStates.visitBackwards(trackProxy.tipIndex(), [&](const auto& state) {
            int measurementId = -1;
            if (state.hasUncalibratedSourceLink()) {
                const auto index = state.getUncalibratedSourceLink()
                    .template get<ActsExamples::IndexSourceLink>().index();
                if (index < trackSeed.getMeasurements().size()) {
                    measurementId = trackSeed.getMeasurements()[index].getObjectID().index;
                }
            }
            for (const std::string stage : {"predicted", "filtered", "smoothed"}) {
                if ((stage == "predicted" && !state.hasPredicted()) ||
                    (stage == "filtered" && !state.hasFiltered()) ||
                    (stage == "smoothed" && (!smoothed || !state.hasSmoothed()))) {
                    continue;
                }
                const Acts::BoundVector parameters = stage == "predicted" ? state.predicted() :
                    stage == "filtered" ? state.filtered() : state.smoothed();
                if (!parameters.allFinite() || parameters[Acts::eBoundQOverP] == 0 ||
                    parameters[Acts::eBoundTheta] <= 0 || parameters[Acts::eBoundTheta] >= M_PI) {
                    continue;
                }
                const Acts::BoundTrackParameters bound(state.referenceSurface().getSharedPtr(),
                    parameters, std::nullopt, trackProxy.particleHypothesis());
                const Acts::Vector3 position = bound.position(geoContext) / Acts::UnitConstants::mm;
                const Acts::Vector3 momentum = bound.momentum() / Acts::UnitConstants::GeV;
                m_log->info("KF_POINT version=1 run={} event={} seed={} state={} surface={} "
                    "meas_id={} measurement={} outlier={} hole={} stage={} path_mm={:.17g} "
                    "x={:.17g} y={:.17g} z={:.17g} px={:.17g} py={:.17g} pz={:.17g} "
                    "loc0={:.17g} loc1={:.17g} phi={:.17g} theta={:.17g} qop={:.17g} time_ns={:.17g}",
                    runNumber, eventNumber, trackSeed.id().index, state.index(),
                    state.referenceSurface().geometryId().value(), measurementId,
                    int(state.typeFlags().test(Acts::TrackStateFlag::MeasurementFlag)),
                    int(state.typeFlags().test(Acts::TrackStateFlag::OutlierFlag)),
                    int(state.typeFlags().test(Acts::TrackStateFlag::HoleFlag)), stage,
                    state.pathLength() / Acts::UnitConstants::mm,
                    position.x(), position.y(), position.z(), momentum.x(), momentum.y(), momentum.z(),
                    parameters[Acts::eBoundLoc0] / Acts::UnitConstants::mm,
                    parameters[Acts::eBoundLoc1] / Acts::UnitConstants::mm,
                    parameters[Acts::eBoundPhi], parameters[Acts::eBoundTheta],
                    parameters[Acts::eBoundQOverP] * Acts::UnitConstants::GeV,
                    parameters[Acts::eBoundTime] / Acts::UnitConstants::ns);
            }
        });
    }

    for (auto state : trackProxy.trackStatesReversed()) {
        if ((m_cfg_diagnosticEvent() == -2 || (m_cfg_diagnosticEvent() >= 0 &&
            eventNumber == static_cast<uint64_t>(m_cfg_diagnosticEvent()))) &&
            state.hasCalibrated() && state.calibratedSize() == 2 && state.hasPredicted()) {
            const Acts::Vector2 residual = state.calibrated<2>() - state.predicted().head<2>();
            const Acts::SquareMatrix2 innovation = state.predictedCovariance().topLeftCorner<2, 2>() +
                                                   state.calibratedCovariance<2>();
            const double chi2 = residual.dot(innovation.ldlt().solve(residual));
            const auto index = state.getUncalibratedSourceLink().get<ActsExamples::IndexSourceLink>().index();
            const auto raw = trackSeed.getMeasurements()[index].getHits()[0].getRawHit();
            Acts::Vector2 smoothedResidual = Acts::Vector2::Constant(NAN);
            if (state.hasSmoothed()) {
                smoothedResidual = state.calibrated<2>() - state.smoothed().head<2>();
            }
            m_log->info("KF_STATE event={} seed={} state={} ring={} pad={} plane={} candidate={} "
                        "measurement={} outlier={} hole={} residual_u={} residual_z={} "
                        "innovation_chi2={} state_chi2={} qop_pred={} qop_filtered={} qop_smoothed={} "
                        "meas_id={} candidates={} predicted_u={} predicted_z={} "
                        "pred_cov_uu={} pred_cov_uz={} pred_cov_zz={} "
                        "smoothed_residual_u={} smoothed_residual_z={}",
                        eventNumber, trackSeed.id().index, state.index(), raw.getRing(), raw.getPad(),
                        raw.getPlane(), index,
                        state.typeFlags().test(Acts::TrackStateFlag::MeasurementFlag),
                        state.typeFlags().test(Acts::TrackStateFlag::OutlierFlag),
                        state.typeFlags().test(Acts::TrackStateFlag::HoleFlag), residual[0], residual[1],
                        chi2, state.chi2(), state.predicted()[Acts::eBoundQOverP],
                        state.hasFiltered() ? state.filtered()[Acts::eBoundQOverP] : NAN,
                        state.hasSmoothed() ? state.smoothed()[Acts::eBoundQOverP] : NAN,
                        trackSeed.getMeasurements()[index].getObjectID().index,
                        calibrator.candidateCount(state.referenceSurface().geometryId()),
                        state.predicted()[Acts::eBoundLoc0], state.predicted()[Acts::eBoundLoc1],
                        state.predictedCovariance()(0, 0), state.predictedCovariance()(0, 1),
                        state.predictedCovariance()(1, 1), smoothedResidual[0], smoothedResidual[1]);
        }
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

    // Primary output first; cylinder state also retained after a successful extrapolation.
    auto writeParameters = [&](const Acts::BoundTrackParameters& fitted, int type) {
        const auto& boundParams = fitted.parameters();
        const auto& boundCov = *fitted.covariance();
        auto trackParams = m_out_track_params()->create();
        trackParams.setType(type);
        trackParams.setSurface(fitted.referenceSurface().geometryId().value());
        trackParams.setLoc({
            static_cast<float>(boundParams[Acts::eBoundLoc0] / Acts::UnitConstants::mm),
            static_cast<float>(boundParams[Acts::eBoundLoc1] / Acts::UnitConstants::mm)});
        trackParams.setTheta(static_cast<float>(fitted.theta()));
        trackParams.setPhi(static_cast<float>(fitted.phi()));
        trackParams.setQOverP(static_cast<float>(fitted.qOverP() * Acts::UnitConstants::GeV));
        trackParams.setTime(static_cast<float>(fitted.time() / Acts::UnitConstants::ns));
        trackParams.setPdg(static_cast<int32_t>(fitted.particleHypothesis().absolutePdg()));

        // =========================================================================
        // Full 6x6 output covariance with unit conversion (ACTS internal → EDM)
        //
        // ACTS internal units:  length in mm, angles in rad, q/p in 1/GeV, time in c*t
        // EDM units:            mm, rad, 1/GeV, ns; scale factors applied per-element
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
    };
    writeParameters(outputParameters, perigeeReached ? 0 : 1);
    if (perigeeReached) {
        writeParameters(cylinderParameters, 1);
    }

    auto track = m_out_tracks()->create();
    // 0 = fitted beamline perigee; 1 = cylinder fallback after extrapolation failure.
    track.setType(perigeeReached ? 0 : 1);
    const auto& boundParams = outputParameters.parameters();
    const auto& boundCov = *outputParameters.covariance();
    const Acts::Vector3 position = outputParameters.position(geoContext) / Acts::UnitConstants::mm;
    const Acts::Vector3 momentum = outputParameters.momentum() / Acts::UnitConstants::GeV;
    track.setPosition({static_cast<float>(position.x()), static_cast<float>(position.y()),
                       static_cast<float>(position.z())});
    track.setMomentum({static_cast<float>(momentum.x()), static_cast<float>(momentum.y()),
                       static_cast<float>(momentum.z())});

    // Preserve all correlations with the surface-specific position Jacobian.
    // In particular, d(p_i)/d(q/p) = -p_i/(q/p), including the charge sign.
    const auto jacobian = boundToCartesianJacobian(outputParameters, geoContext);
    const Acts::ActsSquareMatrix<6> cartCov = jacobian * boundCov * jacobian.transpose();
    tdis::Cov6f pmCov;
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            pmCov(i, j) = static_cast<float>(cartCov(i, j));
        }
    }
    track.setPositionMomentumCovariance(pmCov);
    track.setTime(static_cast<float>(boundParams[Acts::eBoundTime] / Acts::UnitConstants::ns));
    track.setTimeError(
        static_cast<float>(std::sqrt(boundCov(Acts::eBoundTime, Acts::eBoundTime)) /
                           Acts::UnitConstants::ns));
    track.setCharge(outputParameters.charge());
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
        "Track successfully converted: p={:.3f} GeV, theta={:.3f} rad, phi={:.3f} rad, chi2/ndf={:.2f}",
        momentum.norm(),
        outputParameters.theta(),
        outputParameters.phi(),
        track.getChi2() / track.getNdf());
}


void KalmanFittingFactory::Execute(int32_t run_number, uint64_t event_number) {
    using namespace Acts::UnitLiterals;

    for (const auto& trackSeed : *m_in_trackSeeds()) {
        processTrack(trackSeed, run_number, event_number);
    }
}

} // namespace tdis::tracking
