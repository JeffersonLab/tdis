#include "KalmanFittingFactory.h"

#include <Acts/Definitions/Units.hpp>
#include <Acts/Utilities/Helpers.hpp>
#include <ActsExamples/EventData/IndexSourceLink.hpp>

#include "ActsLogHeplers.h"
#include "ConfiguredKalmanFitter.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/DigitizedMtpcMcTrackCollection.h"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/TrackCollection.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrajectoryCollection.h"
#include "Acts/Plugins/Podio/PodioTrackContainer.hpp"
#include "Acts/Plugins/Podio/PodioTrackStateContainer.hpp"
#include "Acts/Plugins/Podio/PodioUtil.hpp"

namespace tdis::tracking {

KalmanFittingFactory::KalmanFittingFactory() {

}


struct MapHelper : public Acts::PodioUtil::ConversionHelper {
    std::optional<Acts::PodioUtil::Identifier> surfaceToIdentifier(const Acts::Surface& surface) const override {
        for (auto&& [id, srf] : surfaces) {
            if (srf == &surface) {
                return id;
            }
        }
        return {};
    }

    const Acts::Surface* identifierToSurface(Acts::PodioUtil::Identifier id) const override {
        auto it = surfaces.find(id);
        if (it == surfaces.end()) {
            return nullptr;
        }

        return it->second;
    }

    Acts::PodioUtil::Identifier sourceLinkToIdentifier(const Acts::SourceLink& sl) override {
        sourceLinks.push_back(sl);
        return sourceLinks.size() - 1;
    }

    Acts::SourceLink identifierToSourceLink(Acts::PodioUtil::Identifier id) const override {
        return sourceLinks.at(id);
    }

    std::unordered_map<Acts::PodioUtil::Identifier, const Acts::Surface*> surfaces;
    std::vector<Acts::SourceLink> sourceLinks;
};

struct Factory {
    using trajectory_t = Acts::MutablePodioTrackStateContainer;
    using const_trajectory_t = Acts::ConstPodioTrackStateContainer;

    MapHelper m_helper;

    Acts::MutablePodioTrackStateContainer create() {
        return Acts::MutablePodioTrackStateContainer{m_helper};
    }
};


void KalmanFittingFactory::Configure() {
    // Setup magnetic field
    auto magneticField = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0, 0, m_bz() * Acts::UnitConstants::T));

    // Configure EigenStepper, Navigator, and Propagator
    Stepper stepper(magneticField);
    Acts::Navigator::Config navCfg{m_acts_geo_svc->GetTrackingGeometry()};
    Acts::Navigator navigator(navCfg);
    m_propagator = std::make_shared<Propagator>(stepper, std::move(navigator));

    // Initialize KalmanFitter
    m_kalman_fitter = std::make_shared<KF>(*m_propagator);

    m_logger = m_log_svc->logger("tracking/kf");



    // ---------- CSV ----------
    m_csv.open(m_csv_out(), std::ios::out | std::ios::trunc);
    if (!m_csv) {
        throw std::runtime_error("Cannot open CSV output file " + m_csv_out());
    }
    m_csv << "#evt, mc_p_GeV, mc_theta_deg, mc_phi_deg,"
             "reco_p_GeV, reco_theta_deg, reco_phi_deg,"
             "chi2, ndof, chi2_per_ndof\n";

    // ---------- ACTS logger ----------
    auto lvl = strToActsLevel(m_acts_level());
    m_acts_logger = Acts::getDefaultLogger("KF", lvl);


    m_fitter = ActsExamples::makeKalmanFitterFunction(
        m_acts_geo_svc->GetTrackingGeometry(),
        magneticField,
        true, // multipleScattering
        true, // energyLoss
        0.0, // reverseFilteringMomThreshold
        Acts::FreeToBoundCorrection(), // FreeToBoundCorrection
        *m_acts_logger   // logger
    );
}

void KalmanFittingFactory::Execute(int32_t run_number, uint64_t event_number) {
    using namespace Acts::UnitLiterals;

    auto geometry = m_acts_geo_svc->GetTrackingGeometry();


    // Retrieve input data
    const auto& mcTracks = *m_mc_tracks_input();
    const auto& measurements = *m_measurements_input();

    // Contexts
    auto geoContext = m_acts_geo_svc->GetActsGeometryContext();
    Acts::MagneticFieldContext magContext;
    Acts::CalibrationContext calibContext;

    ActsExamples::ConfiguredFitter::GeneralFitterOptions general_fitter_options = {
        .geoContext = geoContext,
        .magFieldContext = magContext,
        .calibrationContext = calibContext,
        .propOptions = Acts::PropagatorPlainOptions(geoContext, magContext)

    };

    // ActsExamples::PassThroughCalibrator calibrator;

    // Prepare output containers
    auto trackContainer = std::make_shared<Acts::VectorTrackContainer>();
    auto trackStateContainer = std::make_shared<Acts::VectorMultiTrajectory>();
    Acts::TrackContainer tracks(trackContainer, trackStateContainer);

    using trajectory_t = Acts::MutablePodioTrackStateContainer;
    using const_trajectory_t = Acts::ConstPodioTrackStateContainer;
    Acts::MutablePodioTrackStateContainer t;


    // KalmanFitter extensions with default components
    Acts::KalmanFitterExtensions<Acts::VectorMultiTrajectory> extensions;

    // KalmanFitter options
    Acts::KalmanFitterOptions kfOptions(
        geoContext,
        magContext,
        calibContext,
        extensions,
        Acts::PropagatorPlainOptions(geoContext, magContext),
        nullptr,    // No reference surface
        true,       // Multiple scattering
        true        // Energy loss
    );


    // Process each MC track
    for (const auto& mcTrack : mcTracks) {

        // create sourcelink and measurement containers
        auto actsMeasurements = std::make_shared<ActsExamples::MeasurementContainer>();

        int track_start_ring = 999999999;
        double track_start_x = 0;
        double track_start_y = 0;
        double track_start_z = 0;

        // Collect SourceLinks for this track's measurements
        std::vector<Acts::SourceLink> sourceLinks;
        m_logger->info("Track id={} colId={} Hits:", mcTrack.id().index, mcTrack.id().collectionID);
        for (const auto& mcHit : mcTrack.hits()) {
            for (size_t i = 0; i < measurements.size(); ++i) {
                const auto& measurement = measurements[i];
                if (!measurement.hits().empty() && measurement.hits().at(0).rawHit().id() == mcHit.id()) { // Compare ids


                    auto x = (double)mcHit.truePosition().x;
                    auto y = (double)mcHit.truePosition().y;
                    auto z = (double)mcHit.truePosition().z;

                    auto reconstructedHit = measurement.hits().at(0);


                    if (mcHit.ring() < track_start_ring) {

                        track_start_x = reconstructedHit.position().x;
                        track_start_y = reconstructedHit.position().y;
                        track_start_z = reconstructedHit.position().z;
                        track_start_ring = mcHit.ring();
                    }

                    m_logger->info("    id={}-{}, plane={}, ring={}, pad={}, x={}, y={}, z={}, surf-id={}",
                        mcHit.id().collectionID, mcHit.id().index,
                        mcHit.plane(), mcHit.ring(), mcHit.pad(),
                        x, y, z, measurement.surface());

                    // This is a test that surfaces we think we have, are in tracking geometry
                    auto surfaceFromTrkGeo = geometry->findSurface(Acts::GeometryIdentifier(measurement.surface()));
                    auto surfaceGeoId = surfaceFromTrkGeo->geometryId();
                    if (!surfaceFromTrkGeo) {
                        auto msg = fmt::format("For ring = {}, we can't find back the surface with id = {}. It is trackingGeometry->findSurface==NULL. Track fitting will fail soon (!)", mcHit.ring(), surfaceGeoId.volume());
                        m_logger->critical(msg);
                        throw std::runtime_error(msg);
                    }

                    ActsExamples::IndexSourceLink sourceLink(surfaceGeoId, i);

                    sourceLinks.emplace_back(sourceLink);


                    // 1) Prepare the data vector (size=2)
                    Acts::Vector2 loc2D = Acts::Vector2::Zero();
                    loc2D[Acts::eBoundLoc0] = measurement.loc().a;
                    loc2D[Acts::eBoundLoc1] = measurement.loc().b;

                    // 2) Prepare the 2x2 covariance
                    Acts::SquareMatrix2 cov2D = Acts::SquareMatrix2::Zero();
                    cov2D(0, 0) = measurement.covariance().xx;
                    cov2D(1, 1) = measurement.covariance().yy;
                    cov2D(0, 1) = measurement.covariance().xy;
                    cov2D(1, 0) = measurement.covariance().xy;

                    //auto actsMeasurement = actsMeasurements->makeMeasurement<Acts::eBoundSize>(geoId);

                    actsMeasurements->emplaceMeasurement<2>(
                        surfaceGeoId,
                        std::array{Acts::eBoundLoc0, Acts::eBoundLoc1},  // Subspace indices FIRST
                        loc2D,                                    // Parameters vector
                        cov2D                                     // Covariance matrix
                    );
                    break;
                 }
            }
        }


        if (sourceLinks.empty()) {
            m_logger->warn("Track {} has no measurements", mcTrack.id().index);
            continue;
        }

        // Convert truth parameters to initial parameters
        const double p = mcTrack.momentum() * 1_GeV;
        const double theta = mcTrack.theta() * Acts::UnitConstants::degree;
        const double phi = mcTrack.phi() * Acts::UnitConstants::degree;
        const double vz = mcTrack.vertexZ();

        // Create initial parameters at perigee
        auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(
             Acts::Vector3(track_start_x, track_start_y, track_start_z)
        );

        // auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(
        //     Acts::Vector3(0,0,0)
        // );
        Acts::BoundVector params = Acts::BoundVector::Zero();
        params[Acts::eBoundPhi] = phi;
        params[Acts::eBoundTheta] = theta;
        params[Acts::eBoundQOverP] = 1.0 / p;

        Acts::BoundTrackParameters startParams(
            perigee, params, Acts::BoundMatrix::Identity(),
            Acts::ParticleHypothesis::proton()
        );

        m_logger->info("Initial track parameters: p = {:.3f} GeV, theta = {:.3f} deg, phi = {:.3f} deg, vz = {:.3f} mm",
               p / Acts::UnitConstants::GeV, theta / Acts::UnitConstants::degree,
               phi / Acts::UnitConstants::degree, vz);


        ActsExamples::PassThroughCalibrator pcalibrator;
        ActsExamples::MeasurementCalibratorAdapter calibrator(pcalibrator, *actsMeasurements);

        // Run Kalman fit
        //auto result = m_kalman_fitter->fit(
        //            sourceLinks.begin(), sourceLinks.end(), startParams, kfOptions, tracks
        //);
        // 7) Run the Kalman fit => result
        auto result = (*m_fitter)(sourceLinks, startParams, general_fitter_options, calibrator, tracks);
        if (!result.ok()) {
            m_logger->error("Fit failed for track {}: {}", mcTrack.id().index, 
                            result.error().message());
            continue;
        } else {
            // If you want to do anything with the resulting track proxy right now,
            // you can retrieve it (but it's already in 'tracks'):
            auto& trackProxy = result.value();
            auto tip = trackProxy.tipIndex();
            auto absMom = trackProxy.absoluteMomentum();
            m_logger->info("mcTrack.mom = {} reco mom = {}", mcTrack.momentum(), absMom);
            m_logger->info("mcTrack.theta = {} reco = {}", mcTrack.theta(), trackProxy.theta());
            m_logger->info("mcTrack.phi  = {} reco phi {}", mcTrack.theta(), trackProxy.phi());
            m_logger->info("reco chi2 {} nDoF {} chi2/ndof {}", trackProxy.chi2(), trackProxy.nDoF(), trackProxy.chi2()/ trackProxy.nDoF());

            m_logger->debug("Successfully fitted track => track p {} in container",
                            trackProxy.absoluteMomentum());
        }

        if (!result.ok()) {
            m_logger->error("Fit failed for track {}: {}", mcTrack.id().index, result.error().message());
        }
        // TODO we should end here
    }

    // Store results
    ActsExamples::ConstTrackContainer constTracks{
        std::make_shared<Acts::ConstVectorTrackContainer>(std::move(*trackContainer)),
        std::make_shared<Acts::ConstVectorMultiTrajectory>(std::move(*trackStateContainer))
    };
    // ======== BEGIN EDM4eic Conversion ======== //
    constexpr std::array<std::pair<Acts::BoundIndices, double>, 6> edm4eic_indexed_units{{
        {Acts::eBoundLoc0, Acts::UnitConstants::mm},
        {Acts::eBoundLoc1, Acts::UnitConstants::mm},
        {Acts::eBoundPhi, 1.},
        {Acts::eBoundTheta, 1.},
        {Acts::eBoundQOverP, 1./Acts::UnitConstants::GeV},
        {Acts::eBoundTime, Acts::UnitConstants::ns}
    }};

    // Loop over ACTS tracks
    for (const auto& track : constTracks) {
        auto trajectory = m_edm_trajectories()->create();
        auto edmTrackParams = m_edm_track_params()->create();
        auto edmTrack = m_edm_tracks()->create();

        // Convert track parameters
        if (track.hasReferenceSurface()) {
            const auto& params = track.parameters();
            edmTrackParams.loc({static_cast<float>(params[Acts::eBoundLoc0]),
                                   static_cast<float>(params[Acts::eBoundLoc1])});
            edmTrackParams.theta(params[Acts::eBoundTheta]);
            edmTrackParams.phi(params[Acts::eBoundPhi]);
            edmTrackParams.qOverP(params[Acts::eBoundQOverP]);
            edmTrackParams.time(params[Acts::eBoundTime]);

            edm4eic::Cov6f cov;
            for (size_t i=0; auto& [idx, scale] : edm4eic_indexed_units) {
                for (size_t j=0; auto& [jdx, jscale] : edm4eic_indexed_units) {
                    cov(i,j) = track.covariance()(idx,jdx) * scale * jscale;
                    ++j;
                }
                ++i;
            }
            edmTrackParams.covariance(cov);
        }

        // Associate measurements
        for (const auto& state : track.trackStatesReversed()) {
            if (state.hasUncalibratedSourceLink()) {
                const auto& sl = state.getUncalibratedSourceLink().template get<ActsExamples::IndexSourceLink>();
                if (state.typeFlags().test(Acts::TrackStateFlag::MeasurementFlag)) {
                    if (sl.index() < m_measurements_input()->size()) {
                        auto meas = (*m_measurements_input())[sl.index()];
                        edmTrack.addmeasurements(meas);
                    }
                }
            }
        }

        trajectory.addtrackParameters(edmTrackParams);
        edmTrack.trajectory(trajectory);

        edmTrack.chi2(track.chi2());
        edmTrack.ndf(track.nDoF());
        edmTrack.charge(track.qOverP() > 0 ? 1 : -1);
        edmTrack.pdg(track.particleHypothesis().absolutePdg());
    }
    // ======== END EDM4eic Conversion ======== //
}

} // namespace tdis::tracking