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
    auto lvl = strToActsLevel(m_acts_level());
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
            "   id={}, plane={:<4} ring={:<3} pad={:<3}, x={:<6.2f} y={:<6.2f}, z={:<6.2f}, "
            "surf-id={}",
            mcHit.id().index, mcHit.getPlane(), mcHit.getRing(), mcHit.getPad(), podioPerigee.x,
            podioPerigee.y, podioPerigee.z, measurement.getSurface());

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


    Acts::BoundVector initBounds = Acts::BoundVector::Zero();
    initBounds[Acts::eBoundPhi] = podioInitParams.getPhi();
    initBounds[Acts::eBoundTheta] = podioInitParams.getTheta();
    initBounds[Acts::eBoundQOverP] = podioInitParams.getQOverP();

    Acts::BoundTrackParameters actsInitParams(
        perigee,
        initBounds,
        Acts::BoundMatrix::Identity(),
        Acts::ParticleHypothesis::proton()
    );

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

    // Options and contexts
    // Contexts
    auto geoContext = m_acts_geo_svc->GetActsGeometryContext();
    Acts::MagneticFieldContext magContext;
    Acts::CalibrationContext calibContext;



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

    if (doRefit) {
        kfOptions.extensions.surfaceAccessor.connect<&tdis::RefittingCalibrator::accessSurface>();
    } else {
        kfOptions.extensions.surfaceAccessor.connect<&ActsExamples::IndexSourceLink::SurfaceAccessor::operator()>(&slSurfaceAccessor);
    }

    // Output trajectories
    // using trajectory_t = Acts::MutablePodioTrackStateContainer;
    // using const_trajectory_t = Acts::ConstPodioTrackStateContainer;
    // PodioSurfaceIdConverter surfaceToIdConverter;
    // Acts::MutablePodioTrackStateContainer trackStateContainer{surfaceToIdConverter};
    // Acts::MutablePodioTrackContainer trackContainer{surfaceToIdConverter};
    //
    // Acts::TrackContainer tracks(trackContainer, trackStateContainer);

    // Magnetic field map

    // Setup magnetic field
    auto magneticField = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0, 0, m_cfg_bz() * Acts::UnitConstants::T));

    // Configure EigenStepper, Navigator, and Propagator
    //~!tdis::Stepper stepper(magneticField);
    //! Acts::Navigator::Config navCfg{m_acts_geo_svc->GetTrackingGeometry()};
    //! Acts::Navigator navigator(navCfg);
    // !m_propagator = std::make_shared<Propagator>(stepper, std::move(navigator));

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
                actsInitParams,
                kfOptions,
                tracks);

    //auto result = trackFitter->fit(
//                sourceLinks.begin(), sourceLinks.end(), startParams, kfOptions, tracks
//    );
    // 7) Run the Kalman fit => result
    // auto result = (*m_fitter)(sourceLinks, startParams, general_fitter_options, calibrator, tracks);
    if (!result.ok()) {
        m_log->error("Fit failed for trackSeed {}: {}", trackSeed.id().index, result.error().message());
    //     continue;
    } else {
        // If you want to do anything with the resulting track proxy right now,
        // you can retrieve it (but it's already in 'tracks'):
        auto& trackProxy = result.value();
        auto tip = trackProxy.tipIndex();
        auto absMom = trackProxy.absoluteMomentum();
        auto mcTrack = trackSeed.getMcTrack();
        m_log->info("trackProxy.tipIndex() = {}", trackProxy.tipIndex());
        m_log->info("mcTrack.mom = {} reco mom = {}", mcTrack.getMomentum(), absMom);
        m_log->info("mcTrack.theta = {} reco = {}", mcTrack.getTheta(), trackProxy.theta());
        m_log->info("mcTrack.phi  = {} reco phi {}", mcTrack.getTheta(), trackProxy.phi());
        m_log->info("reco chi2 {} nDoF {} chi2/ndof {}", trackProxy.chi2(), trackProxy.nDoF(), trackProxy.chi2()/ trackProxy.nDoF());

        m_log->debug("Successfully fitted track => track p {} in container",
                        trackProxy.absoluteMomentum());
    }
    //
    // if (!result.ok()) {
    //     m_logger->error("Fit failed for track {}: {}", mcTrack.id().index, result.error().message());
    // }
    // TODO we should end here
}


void KalmanFittingFactory::Execute(int32_t run_number, uint64_t event_number) {
    using namespace Acts::UnitLiterals;

    auto geometry = m_acts_geo_svc->GetTrackingGeometry();



    // ActsExamples::PassThroughCalibrator calibrator;

    // Prepare output containers
    //auto trackContainer = std::make_shared<Acts::VectorTrackContainer>();
    //auto trackStateContainer = std::make_shared<Acts::VectorMultiTrajectory>();


    // using trajectory_t = Acts::MutablePodioTrackStateContainer;
    // using const_trajectory_t = Acts::ConstPodioTrackStateContainer;
    // PodioSurfaceIdConverter surfaceToIdConverter;
    // Acts::MutablePodioTrackStateContainer trackStateContainer{surfaceToIdConverter};
    // Acts::MutablePodioTrackContainer trackContainer{surfaceToIdConverter};
    //
    // Acts::TrackContainer tracks(trackContainer, trackStateContainer);
    //
    // // KalmanFitter extensions with default componentss
    // Acts::KalmanFitterExtensions<Acts::VectorMultiTrajectory> kfExtensions;
    //
    // // KalmanFitter options
    // Acts::KalmanFitterOptions kfOptions(
    //     geoContext,
    //     magContext,
    //     calibContext,
    //     kfExtensions,
    //     Acts::PropagatorPlainOptions(geoContext, magContext),
    //     nullptr,    // No reference surface
    //     true,       // Multiple scattering
    //     true        // Energy loss
    // );


    // Process each MC track
    for (const auto& trackSeed: *m_in_trackSeeds()) {

        processTrack(trackSeed);
    }

    // // Store results
    // ActsExamples::ConstTrackContainer constTracks{
    //     std::make_shared<Acts::ConstPodioTrackContainer>(std::move(*trackContainer)),
    //     std::make_shared<Acts::ConstPodioTrackStateContainer>(std::move(*trackStateContainer))
    // };
    // // ======== BEGIN EDM4eic Conversion ======== //
    // constexpr std::array<std::pair<Acts::BoundIndices, double>, 6> edm4eic_indexed_units{{
    //     {Acts::eBoundLoc0, Acts::UnitConstants::mm},
    //     {Acts::eBoundLoc1, Acts::UnitConstants::mm},
    //     {Acts::eBoundPhi, 1.},
    //     {Acts::eBoundTheta, 1.},
    //     {Acts::eBoundQOverP, 1./Acts::UnitConstants::GeV},
    //     {Acts::eBoundTime, Acts::UnitConstants::ns}
    // }};

    // Loop over ACTS tracks
    //for (const auto& track : constTracks) {
        // auto trajectory = m_edm_trajectories()->create();
        // auto edmTrackParams = m_edm_track_params()->create();
        // auto edmTrack = m_edm_tracks()->create();
        //
        // // Convert track parameters
        // if (track.hasReferenceSurface()) {
        //     const auto& params = track.parameters();
        //     edmTrackParams.loc({static_cast<float>(params[Acts::eBoundLoc0]),
        //                            static_cast<float>(params[Acts::eBoundLoc1])});
        //     edmTrackParams.theta(params[Acts::eBoundTheta]);
        //     edmTrackParams.phi(params[Acts::eBoundPhi]);
        //     edmTrackParams.qOverP(params[Acts::eBoundQOverP]);
        //     edmTrackParams.time(params[Acts::eBoundTime]);
        //
        //     tdis::Cov6f cov;
        //     for (size_t i=0; auto& [idx, scale] : edm4eic_indexed_units) {
        //         for (size_t j=0; auto& [jdx, jscale] : edm4eic_indexed_units) {
        //             cov(i,j) = track.covariance()(idx,jdx) * scale * jscale;
        //             ++j;
        //         }
        //         ++i;
        //     }
        //     edmTrackParams.covariance(cov);
        // }
        //
        // // Associate measurements
        // for (const auto& state : track.trackStatesReversed()) {
        //     if (state.hasUncalibratedSourceLink()) {
        //         const auto& sl = state.getUncalibratedSourceLink().template get<ActsExamples::IndexSourceLink>();
        //         if (state.typeFlags().test(Acts::TrackStateFlag::MeasurementFlag)) {
        //             if (sl.index() < m_measurements_input()->size()) {
        //                 auto meas = (*m_measurements_input())[sl.index()];
        //                 edmTrack.addmeasurements(meas);
        //             }
        //         }
        //     }
        // }
        //
        // trajectory.addtrackParameters(edmTrackParams);
        // edmTrack.trajectory(trajectory);
        //
        // edmTrack.chi2(track.chi2());
        // edmTrack.ndf(track.nDoF());
        // edmTrack.charge(track.qOverP() > 0 ? 1 : -1);
        // edmTrack.pdg(track.particleHypothesis().absolutePdg());
    //}
    // ======== END EDM4eic Conversion ======== //
}

} // namespace tdis::tracking