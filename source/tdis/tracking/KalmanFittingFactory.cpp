#include "KalmanFittingFactory.h"
#include <Acts/Definitions/Units.hpp>
#include <Acts/Utilities/Helpers.hpp>
#include <ActsExamples/EventData/IndexSourceLink.hpp>

#include "podio_model/Measurement2DCollection.h"
#include "podio_model/TrackCollection.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrajectoryCollection.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/DigitizedMtpcMcTrackCollection.h"

namespace tdis::tracking {

KalmanFittingFactory::KalmanFittingFactory() {

}

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
    // TODO connect spdlog with ACTS logger m_acts_logger = Acts::getDefaultLogger("KF", Acts::Logging::INFO, &m_logger->sinks());
    m_acts_logger = Acts::getDefaultLogger("KF", Acts::Logging::INFO);
}

void KalmanFittingFactory::Execute(int32_t run_number, uint64_t event_number) {
    using namespace Acts::UnitLiterals;

    // Retrieve input data
    const auto& mcTracks = *m_mc_tracks_input();
    const auto& measurements = *m_measurements_input();

    // Contexts
    auto geoContext = m_acts_geo_svc->GetActsGeometryContext();
    Acts::MagneticFieldContext magContext;
    Acts::CalibrationContext calibContext;

    // Prepare output containers
    auto trackContainer = std::make_shared<Acts::VectorTrackContainer>();
    auto trackStateContainer = std::make_shared<Acts::VectorMultiTrajectory>();
    Acts::TrackContainer tracks(trackContainer, trackStateContainer);

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
        // Convert truth parameters to initial parameters
        const double p = mcTrack.momentum() * 1_GeV;
        const double theta = mcTrack.theta() * Acts::UnitConstants::degree;
        const double phi = mcTrack.phi() * Acts::UnitConstants::degree;
        const double vz = mcTrack.vertexZ() * 1_m;

        // Create initial parameters at perigee
        auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(
            Acts::Vector3(0, 0, vz)
        );
        Acts::BoundVector params = Acts::BoundVector::Zero();
        params[Acts::eBoundPhi] = phi;
        params[Acts::eBoundTheta] = theta;
        params[Acts::eBoundQOverP] = 1.0 / p;

        Acts::BoundTrackParameters startParams(
            perigee, params, Acts::BoundMatrix::Identity(),
            Acts::ParticleHypothesis::proton()
        );

        // Collect SourceLinks for this track's measurements
        std::vector<Acts::SourceLink> sourceLinks;
        for (const auto& mcHit : mcTrack.hits()) {
            m_logger->info("Track id={} colId={}", mcTrack.id().index, mcTrack.id().collectionID);
            for (size_t i = 0; i < measurements.size(); ++i) {
                const auto& measurement = measurements[i];
                if (!measurement.hits().empty() && measurement.hits().at(0).rawHit().id() == mcHit.id()) { // Compare ids

                    auto x = (double)mcHit.truePosition().x;
                    auto y = (double)mcHit.truePosition().y;
                    auto z = (double)mcHit.truePosition().z;

                    m_logger->info("    id={}-{}, plane={}, ring={}, pad={}, x={}, y={}, z={}",
                        mcHit.id().collectionID, mcHit.id().index,
                        mcHit.plane(), mcHit.ring(), mcHit.pad(),
                        x, y, z);
                    auto surfaceId = measurement.surface();
                    ActsExamples::IndexSourceLink sourceLink(mcHit.ring(), i);
                    sourceLinks.emplace_back(sourceLink);
                    break;
                }
            }
        }

        if (sourceLinks.empty()) {
            m_logger->warn("Track {} has no measurements", mcTrack.id().index);
            continue;
        }

        // Run Kalman fit
        auto result = m_kalman_fitter->fit(
            sourceLinks.begin(), sourceLinks.end(), startParams, kfOptions, tracks
        );

        if (!result.ok()) {
            m_logger->error("Fit failed for track {}: {}", mcTrack.id().index, result.error().message());
        }
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