#include "KalmanFittingFactory.h"

#include <Acts/Definitions/Units.hpp>
#include <Acts/Utilities/Helpers.hpp>
#include <ActsExamples/EventData/IndexSourceLink.hpp>
#include <ActsExamples/EventData/MeasurementCalibration.hpp>

#include "Acts/Plugins/Podio/PodioTrackContainer.hpp"
#include "Acts/Plugins/Podio/PodioTrackStateContainer.hpp"
#include "Acts/Plugins/Podio/PodioUtil.hpp"
#include "Acts/TrackFitting/GainMatrixSmoother.hpp"
#include "Acts/TrackFitting/GainMatrixUpdater.hpp"
#include "ActsLogHeplers.h"
#include "PadGeometryHelper.hpp"
#include "PodioSurfaceIdConverter.h"
#include "RefittingCalibrator.h"
#include "TrackingTypes.h"
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
    auto magneticField = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0, 0, m_cfg_bz() * Acts::UnitConstants::T));

    // Configure EigenStepper, Navigator, and Propagator
    Stepper stepper(magneticField);
    Acts::Navigator::Config navCfg{m_acts_geo_svc->GetTrackingGeometry()};
    Acts::Navigator navigator(navCfg);
    m_propagator = std::make_shared<Propagator>(stepper, std::move(navigator));

    // Initialize KalmanFitter
    //m_kalman_fitter = std::make_shared<KF>(*m_propagator);

    // Logging
    m_log = m_log_svc->logger("tracking:kf");

    // Acts logging
    auto actsLvlStr = m_acts_level();
    auto lvl = strToActsLevel(actsLvlStr);
    m_acts_logger = Acts::getDefaultLogger("tracking:kf", lvl);


}

void KalmanFittingFactory::fillMeasurements(
    tdis::TrackSeed trackSeed,
    std::shared_ptr<ActsExamples::MeasurementContainer>& actsMeasurements,
    std::vector<Acts::SourceLink>& sourceLinks) {
    auto geometry = m_acts_geo_svc->GetTrackingGeometry();

    // Contexts
    // auto geoContext = m_acts_geo_svc->GetActsGeometryContext();
    // Acts::MagneticFieldContext magContext;
    // Acts::CalibrationContext calibContext;

    actsMeasurements = std::make_shared<ActsExamples::MeasurementContainer>();

    auto initTrackParams = trackSeed.getInitParams();
    auto podioPerigee = trackSeed.getPerigee();

    for (auto measurement : trackSeed.getMeasurements()) {
        // There always should be at least 1 reconstructed hit attached to measurement
        auto reconstructedHit = measurement.getHits().at(0);
        auto mcHit = reconstructedHit.getRawHit();

        m_log->info(
            "   id={:<4} plane={:<3} ring={:<3} pad={:<3}, x={:<7.2f} y={:<7.2f}, z={:<7.2f}, Perigee.z={:<6.2f} surf-id={}",
            mcHit.id().index, mcHit.getPlane(), mcHit.getRing(), mcHit.getPad(),
            mcHit.getTruePosition().x / Acts::UnitConstants::mm,
            mcHit.getTruePosition().y / Acts::UnitConstants::mm,
            mcHit.getTruePosition().z / Acts::UnitConstants::mm,
            podioPerigee.z / Acts::UnitConstants::mm,
            measurement.getSurface());

        // This is a test that surfaces we think we have, are in tracking geometry
        auto surfaceFromTrkGeo
            = geometry->findSurface(Acts::GeometryIdentifier(measurement.getSurface()));
        auto surfaceGeoId = surfaceFromTrkGeo->geometryId();
        // ReSharper disable once CppDFAConstantConditions
        if (surfaceFromTrkGeo == nullptr) {
            auto msg = fmt::format(
                "For ring = {}, we can't find back the surface with id = {}. It is "
                "trackingGeometry->findSurface==NULL. Track fitting will fail soon (!)",
                mcHit.getRing(), surfaceGeoId.volume());
            m_log->critical(msg);
            throw std::runtime_error(msg);
        }

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

        // auto actsMeasurement = actsMeasurements->makeMeasurement<Acts::eBoundSize>(geoId);

        actsMeasurements->emplaceMeasurement<2>(
            surfaceGeoId, std::array{Acts::eBoundLoc0, Acts::eBoundLoc1},  // Subspace indices FIRST
            loc2D,                                                         // Parameters vector
            cov2D                                                          // Covariance matrix
        );
    }
}


void KalmanFittingFactory::processTrack(tdis::TrackSeed trackSeed) {

    // Options and contexts
    // Contexts
    auto geoContext = m_acts_geo_svc->GetActsGeometryContext();
    Acts::MagneticFieldContext magContext;
    Acts::CalibrationContext calibContext;


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
          Acts::Vector3(podioPerigee.x, podioPerigee.y, podioPerigee.z)
    );

    // auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(
    //     Acts::Vector3(0,0,0)
    // );

    // Fill track initial parameters
    auto podioInitParams = trackSeed.getInitParams();

    const auto& firstMeas = trackSeed.getMeasurements().at(0);


    auto firstMcHit = trackSeed.getMcTrack().getHits().at(0);
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

    //Acts::BoundVector initBounds = Acts::BoundVector::Zero();
    // initBounds[Acts::eBoundPhi] = podioInitParams.getPhi();
    // initBounds[Acts::eBoundTheta] = podioInitParams.getTheta();
    // initBounds[Acts::eBoundQOverP] = podioInitParams.getQOverP();

    // Acts::BoundVector init = Acts::BoundVector::Zero();
    // init[Acts::eBoundLoc0]  = firstMeas.getLoc().a;
    // init[Acts::eBoundLoc1]  = firstMeas.getLoc().b;
    // init[Acts::eBoundPhi]   = podioInitParams.getPhi();
    // init[Acts::eBoundTheta] = podioInitParams.getTheta();
    // init[Acts::eBoundQOverP]= podioInitParams.getQOverP();
    // init[Acts::eBoundTime]  = podioInitParams.getTime();


    // Acts::BoundTrackParameters actsInitParams(
    //     perigee,
    //     initBounds,
    //     Acts::BoundMatrix::Identity(),
    //     Acts::ParticleHypothesis::proton()
    // );


    // Direction from first two true hits (normalize!)
    Eigen::Vector3d dir3{double(secondMcHit.getTruePosition().x-firstMcHit.getTruePosition().x),
                         double(secondMcHit.getTruePosition().y-firstMcHit.getTruePosition().y),
                         double(secondMcHit.getTruePosition().z-firstMcHit.getTruePosition().z)};
    if (dir3.norm() == 0.) {
        m_log->warn("Two identical MC hit positions; cannot define direction.");
        return;
    }
    dir3.normalize();

    // Map your PODIO covariance to ACTS order and init to zero first
    Acts::BoundSquareMatrix actsCov = Acts::BoundSquareMatrix::Zero();
    const auto C = podioInitParams.getCovariance();
    // PODIO order: [loc0, loc1, theta, phi, q/p, time]
    // ACTS order : [loc0, loc1,  phi, theta, q/p, time]
    actsCov(Acts::eBoundLoc0,   Acts::eBoundLoc0)   = C(0,0);
    actsCov(Acts::eBoundLoc1,   Acts::eBoundLoc1)   = C(1,1);
    actsCov(Acts::eBoundPhi,    Acts::eBoundPhi)    = C(3,3);
    actsCov(Acts::eBoundTheta,  Acts::eBoundTheta)  = C(2,2);
    actsCov(Acts::eBoundQOverP, Acts::eBoundQOverP) = C(4,4);
    actsCov(Acts::eBoundTime,   Acts::eBoundTime)   = C(5,5);

    auto alternateInitParams = Acts::BoundTrackParameters::create(
        geoContext,
        startSurf,
        {
            firstMcHit.getTruePosition().x,
            firstMcHit.getTruePosition().y,
            firstMcHit.getTruePosition().z,
            firstMcHit.getTime()
        },
        dir3,
        podioInitParams.getQOverP(),
        actsCov,
        Acts::ParticleHypothesis::proton(),
   2*getPadHight());

    if (!alternateInitParams.ok()) {
        m_log->warn("!alternateInitParams.ok()");
        m_log->warn(alternateInitParams.error().message());
        return;
    }
    // Acts::BoundTrackParameters actsInitParams(
    //      perigee,
    //      initBounds,
    //      Acts::BoundMatrix::Identity(),
    //      Acts::ParticleHypothesis::proton()
    // );

    m_log->info("Initial track parameters: p = {:.3f} GeV, theta = {:.3f} deg, phi = {:.3f} deg, vz = {:.3f} mm",
            1 / podioInitParams.getQOverP() / Acts::UnitConstants::GeV,
            podioInitParams.getTheta() / Acts::UnitConstants::degree,
            podioInitParams.getPhi() / Acts::UnitConstants::degree,
            podioPerigee.z);


    ActsExamples::PassThroughCalibrator calibrator;
    ActsExamples::MeasurementCalibratorAdapter calibratorAdaptor(calibrator, *actsMeasurements);

    // KalmanFitter fitter;
    // DirectKalmanFitter directFitter;

    Acts::GainMatrixUpdater kfUpdater;
    Acts::GainMatrixSmoother kfSmoother;
    SimpleReverseFilteringLogic reverseFilteringLogic;

    bool multipleScattering = false;
    bool energyLoss = false;
    Acts::FreeToBoundCorrection freeToBoundCorrection;

    auto geometry = m_acts_geo_svc->GetTrackingGeometry();
    ActsExamples::IndexSourceLink::SurfaceAccessor slSurfaceAccessor{*geometry};

    // Setup extensions
    Acts::KalmanFitterExtensions<Acts::VectorMultiTrajectory> extensions;
    extensions.updater.connect<&Acts::GainMatrixUpdater::operator()<Acts::VectorMultiTrajectory>>(&kfUpdater);
    extensions.smoother.connect<&Acts::GainMatrixSmoother::operator()<Acts::VectorMultiTrajectory>>(&kfSmoother);
    extensions.reverseFilteringLogic.connect<&SimpleReverseFilteringLogic::doBackwardFiltering>(&reverseFilteringLogic);

    const Acts::Surface* referenceSurface = nullptr;

    bool doRefit = false;

    // Create options
    Acts::KalmanFitterOptions<Acts::VectorMultiTrajectory> kfOptions(
        geoContext, magContext, calibContext, extensions,
        Acts::PropagatorPlainOptions(geoContext, magContext), referenceSurface);

    // Configure options
    kfOptions.referenceSurfaceStrategy = Acts::KalmanFitterTargetSurfaceStrategy::first;
    kfOptions.multipleScattering = multipleScattering;
    kfOptions.energyLoss = energyLoss;
    kfOptions.freeToBoundCorrection = freeToBoundCorrection;
    kfOptions.extensions.calibrator.connect<&ActsExamples::MeasurementCalibratorAdapter::calibrate>(&calibratorAdaptor);
    kfOptions.referenceSurfaceStrategy = Acts::KalmanFitterTargetSurfaceStrategy::last;

    if (doRefit) {
        kfOptions.extensions.surfaceAccessor.connect<&tdis::RefittingCalibrator::accessSurface>();
    } else {
        kfOptions.extensions.surfaceAccessor.connect<&ActsExamples::IndexSourceLink::SurfaceAccessor::operator()>(&slSurfaceAccessor);
    }

    // Magnetic field map

    // Setup magnetic field
    auto magneticField = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0, 0, m_cfg_bz() * Acts::UnitConstants::T));

    // Stepper should be copied into the fitters
    const Stepper stepper(std::move(magneticField));

    // Standard fitter
    Acts::Navigator::Config cfg;
    cfg.trackingGeometry = geometry;
    cfg.resolvePassive = false;
    cfg.resolveMaterial = false;
    cfg.resolveSensitive = true;

    Acts::Navigator navigator(cfg, m_acts_logger->cloneWithSuffix("Navigator"));

    Propagator propagator(stepper, std::move(navigator), m_acts_logger->cloneWithSuffix("Propagator"));
    KalmanFitter trackFitter(std::move(propagator), m_acts_logger->cloneWithSuffix("Fitter"));

    // Direct fitter
    Acts::DirectNavigator directNavigator{m_acts_logger->cloneWithSuffix("DirectNavigator")};
    DirectPropagator directPropagator(stepper, std::move(directNavigator), m_acts_logger->cloneWithSuffix("DirectPropagator"));
    DirectKalmanFitter directTrackFitter(std::move(directPropagator), m_acts_logger->cloneWithSuffix("DirectFitter"));

    Acts::VectorMultiTrajectory vTraj;
    Acts::VectorTrackContainer  vTrk;
    Acts::TrackContainer        tracks(vTrk, vTraj);

    //return kfOptions;

    // Run Kalman fit
    auto result = trackFitter.fit(
                sourceLinks.begin(),
                sourceLinks.end(),
                alternateInitParams.value(),
                kfOptions,
                tracks);

    if (!result.ok()) {
        m_log->error("Fit failed for trackSeed {}: {}", trackSeed.id().index, result.error().message());
        return;

    }
    // If you want to do anything with the resulting track proxy right now,
    // you can retrieve it (but it's already in 'tracks'):
    auto& trackProxy = result.value();
    auto tip = trackProxy.tipIndex();
    auto absMom = trackProxy.absoluteMomentum();
    auto mcTrack = trackSeed.getMcTrack();
    m_log->info("trackProxy.tipIndex() = {}", trackProxy.tipIndex());
    m_log->info("mcTrack.mom = {} reco mom = {}", mcTrack.getMomentum(), absMom);
    m_log->info("mcTrack.theta = {} reco = {}", mcTrack.getTheta(), trackProxy.theta());
    m_log->info("mcTrack.phi  = {} reco phi {}", mcTrack.getPhi(), trackProxy.phi());
    m_log->info("reco chi2 {} nDoF {} chi2/ndof {}", trackProxy.chi2(), trackProxy.nDoF(), trackProxy.chi2()/ trackProxy.nDoF());

    m_log->debug("Successfully fitted track => track p {} in container",
                    trackProxy.absoluteMomentum());

    const auto& params = trackProxy.parameters();
    double qOverP = params[Acts::eBoundQOverP];
    double momentum = std::abs(1.0 / qOverP);
    double theta = params[Acts::eBoundTheta];
    double phi = params[Acts::eBoundPhi];

    m_log->info("qOverP = params[Acts::eBoundQOverP]; = {}", qOverP);
    m_log->info("momentum = std::abs(1.0 / qOverP); = {}", momentum);
    m_log->info("theta = params[Acts::eBoundTheta]; = {}", theta);
    m_log->info("phi = params[Acts::eBoundPhi]; = {}", phi);

    for (auto state : trackProxy.trackStatesReversed()) {
        if (state.hasSmoothed()) {
            m_log->info("Smoothed params at {}: loc0={}, loc1={}, phi={}, theta={}, qOverP={}, momentum={}",
                        state.index(),
                        state.smoothed()[Acts::eBoundLoc0],
                        state.smoothed()[Acts::eBoundLoc1],
                        state.smoothed()[Acts::eBoundPhi],
                        state.smoothed()[Acts::eBoundTheta],
                        state.smoothed()[Acts::eBoundQOverP],
                        1/state.smoothed()[Acts::eBoundQOverP]);
        }
    }

    m_log->info("N STATES: {}", trackProxy.nTrackStates());
    m_log->info("N MEAS: {}", trackProxy.nMeasurements());
    m_log->info("N OUTLIERS: {}", trackProxy.nOutliers());
    m_log->info("N HOLES: {}", trackProxy.nHoles());

    /// ========================== CONVERT TO EDM ====================================


    // Create trajectory summary information
    auto trajectorySummary = Acts::MultiTrajectoryHelpers::trajectoryState(
        tracks.trackStateContainer(), trackProxy.tipIndex());

    // Create the Trajectory object
    auto trajectory = m_out_trajectories->create();
    trajectory.setType(1); // Type 1 = has good track fit
    trajectory.setNStates(trajectorySummary.nStates);
    trajectory.setNMeasurements(trajectorySummary.nMeasurements);
    trajectory.setNOutliers(trajectorySummary.nOutliers);
    trajectory.setNHoles(trajectorySummary.nHoles);
    trajectory.setNSharedHits(trajectorySummary.nSharedHits);
    trajectory.setSeed(trackSeed); // Link back to seed

    // Add chi2 values for measurements and outliers
    for (const auto& measurementChi2 : trajectorySummary.measurementChi2) {
        trajectory.addToMeasurementChi2(measurementChi2);
    }
    for (const auto& outlierChi2 : trajectorySummary.outlierChi2) {
        trajectory.addToOutlierChi2(outlierChi2);
    }

    // Convert track parameters at reference surface
    const auto& boundParams = trackProxy.parameters();
    const auto& boundCov = trackProxy.covariance();

    // Create TrackParameters object (at the track tip/end)
    auto trackParams = m_out_track_params()->create();
    trackParams.setType(0); // Type 0 = track head/tip
    trackParams.setSurface(trackProxy.referenceSurface().geometryId().value());

    // Set track parameters - note the direct access to avoid the absoluteMomentum() issue
    trackParams.setLoc({
        static_cast<float>(boundParams[Acts::eBoundLoc0]),
        static_cast<float>(boundParams[Acts::eBoundLoc1])
    });
    trackParams.setTheta(static_cast<float>(boundParams[Acts::eBoundTheta]));
    trackParams.setPhi(static_cast<float>(boundParams[Acts::eBoundPhi]));
    trackParams.setQOverP(static_cast<float>(boundParams[Acts::eBoundQOverP]));
    trackParams.setTime(static_cast<float>(boundParams[Acts::eBoundTime]));
    trackParams.setPdg(static_cast<int32_t>(trackProxy.particleHypothesis().absolutePdg()));

    // Convert covariance matrix with proper unit conversions
    // Unit conversion factors from ACTS to PODIO
    static constexpr std::array<std::pair<Acts::BoundIndices, double>, 6> edm_indexed_units{
        {{Acts::eBoundLoc0, Acts::UnitConstants::mm},
         {Acts::eBoundLoc1, Acts::UnitConstants::mm},
         {Acts::eBoundPhi, 1.},
         {Acts::eBoundTheta, 1.},
         {Acts::eBoundQOverP, 1. / Acts::UnitConstants::GeV},
         {Acts::eBoundTime, Acts::UnitConstants::ns}}
    };

    tdis::Cov6f cov;
    for (std::size_t i = 0; const auto& [a, x] : edm_indexed_units) {
        for (std::size_t j = 0; const auto& [b, y] : edm_indexed_units) {
            cov(i, j) = boundCov(a, b) / x / y;
            ++j;
        }
        ++i;
    }
    trackParams.setCovariance(cov);

    // Add track parameters to trajectory
    trajectory.addToTrackParameters(trackParams);

    // Create the main Track object
    auto track = m_out_tracks()->create();
    track.setType(trackParams.getType());

    // Calculate momentum from q/p for the track object
    qOverP = boundParams[Acts::eBoundQOverP];
    momentum = std::abs(1.0 / qOverP);
    theta = boundParams[Acts::eBoundTheta];
    phi = boundParams[Acts::eBoundPhi];

    // Convert to Cartesian momentum
    double px = momentum * std::sin(theta) * std::cos(phi);
    double py = momentum * std::sin(theta) * std::sin(phi);
    double pz = momentum * std::cos(theta);

    track.setMomentum({
        static_cast<float>(px * Acts::UnitConstants::GeV),
        static_cast<float>(py * Acts::UnitConstants::GeV),
        static_cast<float>(pz * Acts::UnitConstants::GeV)
    });

    // Set position at vertex (if we have it from perigee or first measurement)
    auto podioOutPerigee = trackSeed.getPerigee();
    track.setPosition({
        static_cast<float>(podioPerigee.x),
        static_cast<float>(podioPerigee.y),
        static_cast<float>(podioPerigee.z)
    });

    // Note: positionMomentumCovariance would need to be transformed from bound to Cartesian
    // For now, leaving it as default zeros
    track.setPositionMomentumCovariance(tdis::Cov6f());

    track.setTime(static_cast<float>(boundParams[Acts::eBoundTime]));
    track.setTimeError(std::sqrt(static_cast<float>(boundCov(Acts::eBoundTime, Acts::eBoundTime))));
    track.setCharge(std::copysign(1.0, qOverP));
    track.setChi2(trajectorySummary.chi2Sum);
    track.setNdf(trajectorySummary.NDF);
    track.setPdg(trackProxy.particleHypothesis().absolutePdg());
    track.setTrajectory(trajectory);

    // Process track states to populate measurements and outliers
    tracks.trackStateContainer().visitBackwards(
        trackProxy.tipIndex(),
        [&](const auto& state) {
            auto typeFlags = state.typeFlags();

            if (state.hasUncalibratedSourceLink()) {
                auto sourceLink = state.getUncalibratedSourceLink()
                    .template get<ActsExamples::IndexSourceLink>();
                std::size_t measIndex = sourceLink.index();

                // Get the original measurement from the seed
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
        }
    );

    // Log the results
    m_log->info("Track successfully converted: p={:.3f} GeV, theta={:.3f} deg, phi={:.3f} deg, chi2/ndf={:.2f}",
                momentum / Acts::UnitConstants::GeV,
                theta / Acts::UnitConstants::degree,
                phi / Acts::UnitConstants::degree,
                track.getChi2() / track.getNdf());
}


void KalmanFittingFactory::Execute(int32_t run_number, uint64_t event_number) {
    using namespace Acts::UnitLiterals;

    auto geometry = m_acts_geo_svc->GetTrackingGeometry();

    // Process each MC track
    for (const auto& trackSeed: *m_in_trackSeeds()) {

        processTrack(trackSeed);

        // Convert to EDM here???
    }
}

} // namespace tdis::tracking