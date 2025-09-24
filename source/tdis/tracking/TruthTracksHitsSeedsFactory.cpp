#include <Acts/Surfaces/CylinderBounds.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>
#include <podio_model/DigitizedMtpcMcTrack.h>
#include <podio_model/DigitizedMtpcMcTrackCollection.h>

#include "TruthTracksHitsSeedsFactory.h"

inline double get_resolution(const double pixel_size) {
    constexpr const double sqrt_12 = 3.4641016151;
    return pixel_size / sqrt_12;
}

inline double get_variance(const double pixel_size) {
    const double res = get_resolution(pixel_size);
    return res * res;
}

namespace tdis::tracking {

    void TruthTracksHitsSeedsFactory::Configure() {
        m_service_geometry();
        m_log = m_service_log->logger("tracking:truth_seed");

        // Initialize random generator
        std::random_device rd;
        m_generator = std::mt19937(rd());
    }

    double TruthTracksHitsSeedsFactory::generateNormal(double mean, double stddev) {
        std::normal_distribution<double> distribution(mean, stddev);
        return distribution(m_generator);
    }

    void TruthTracksHitsSeedsFactory::Execute(int32_t /*run_nr*/, uint64_t event_index) {
        using namespace Acts::UnitLiterals;

        // Create output collections
        auto seeds = std::make_unique<tdis::TrackSeedCollection>();
        auto tracker_hits = std::make_unique<tdis::TrackerHitCollection>();
        auto measurements = std::make_unique<tdis::Measurement2DCollection>();
        auto track_parameters = std::make_unique<tdis::TrackParametersCollection>();

        auto plane_positions = m_service_geometry->GetPlanePositions();

        m_log->trace("TruthTrackSeedFactory, processing event: {}", event_index);

        // Loop over MC tracks
        for (const auto& mc_track: *m_in_tracks()) {

            // Skip tracks without enough hits
            if(mc_track.hits_size() < m_cfg_minHitsForSeed()) {
                m_log->debug("Skipping track with {} hits (< minimum {})",
                    mc_track.hits_size(), m_cfg_minHitsForSeed());
                continue;
            }

            // -------------------------------
            // Step 1: Create TrackerHits and Measurements from MC hits
            // -------------------------------
            std::vector<tdis::TrackerHit> track_hits;
            std::vector<tdis::Measurement2D> track_measurements;

            for (const auto& mcHit : mc_track.getHits()) {
                // Basic geometry indices
                const int plane = mcHit.getPlane();
                const int ring = mcHit.getRing();
                const int pad = mcHit.getPad();
                const double z_to_gem = mcHit.getZToGem();

                if (pad == -999 || pad == 999) {
                    m_log->warn("At event #{} mc Hit had pad 999", event_index);
                    break;
                };

                // Convert ring+pad to (x,y)
                auto [padX, padY] = getPadCenter(ring, pad);
                double planeZ = plane_positions[plane];
                double hitCalcZ = planeZ + (plane % 2 ? -z_to_gem : z_to_gem);

                // Choose position: either true or digitized
                tdis::Vector3f position;
                if (m_cfg_useTrueHitPos() && !std::isnan(mcHit.getTruePosition().x)) {
                    position = mcHit.getTruePosition();
                } else {
                    position.x = static_cast<float>(padX);
                    position.y = static_cast<float>(padY);
                    position.z = static_cast<float>(hitCalcZ);
                }

                // Covariance estimate
                double max_dimension = std::max(getPadApproxWidth(ring), getPadHight());
                double xy_variance = get_variance(max_dimension);
                tdis::CovDiag3f cov{
                    static_cast<float>(xy_variance),
                    static_cast<float>(xy_variance),
                    static_cast<float>(1_cm)
                };

                uint32_t cell_id = 1'000'000 * plane + 1'000 * ring + pad;

                // Create TrackerHit
                auto hit = tracker_hits->create(
                    cell_id,
                    position,
                    cov,
                    static_cast<float>(mcHit.getTime()),
                    static_cast<float>(1_ns),   // Time resolution
                    static_cast<float>(mcHit.getAdc()),
                    0.0F
                );
                hit.setRawHit(mcHit);
                track_hits.push_back(hit);

                // Create Measurement2D
                auto acts_det_element = m_service_geometry().GetDetectorCylinder(ring);
                auto& surfaceRef = acts_det_element->surface();
                auto geometryId = surfaceRef.geometryId().value();

                // Convert to local coordinates
                Acts::Vector2 loc = Acts::Vector2::Zero();
                auto onSurfaceTolerance = 1 * Acts::UnitConstants::mm;

                try {
                    Acts::Vector2 pos = surfaceRef
                        .globalToLocal(
                            Acts::GeometryContext(),
                            {position.x, position.y, planeZ},
                            Acts::Vector3::Zero(),
                            onSurfaceTolerance
                        ).value();

                    loc[Acts::eBoundLoc0] = pos[0];
                    loc[Acts::eBoundLoc1] = pos[1];

                } catch (std::exception& ex) {
                    m_log->warn("Can't convert globalToLocal for hit: plane={} ring={} pad={}, skipping",
                               plane, ring, pad);
                    continue;
                }

                // Create measurement
                auto meas2D = measurements->create();
                meas2D.setSurface(geometryId);
                meas2D.setLoc({static_cast<float>(loc[0]), static_cast<float>(loc[1])});
                meas2D.setTime(hit.getTime());
                meas2D.setCovariance({
                    cov(0, 0),
                    cov(1, 1),
                    hit.getTimeError() * hit.getTimeError(),
                    0.0f  // No off-diagonal for now
                });
                meas2D.addToWeights(1.0);
                meas2D.addToHits(hit);
                track_measurements.push_back(meas2D);
            }

            // Skip if we didn't create enough valid hits/measurements
            if (track_hits.size() < m_cfg_minHitsForSeed()) {
                m_log->debug("Skipping track with {} valid hits after processing", track_hits.size());
                continue;
            }

            // -------------------------------
            // Step 2: Create TrackParameters (from TruthTrackParameterFactory logic)
            // -------------------------------
            auto v = mc_track.getHits().at(0).getTruePosition();  // Use first hit as vertex
            double vx = v.x;
            double vy = v.y;
            double vz = v.z;

            double magnitude = mc_track.getMomentum();
            double theta = mc_track.getTheta();
            double phi = mc_track.getPhi();
            double px = magnitude * std::sin(theta) * std::cos(phi);
            double py = magnitude * std::sin(theta) * std::sin(phi);
            double pz = magnitude * std::cos(theta);

            const auto pmag = std::hypot(px, py, pz);
            const auto pinit = pmag * generateNormal(1, m_cfg_momentumSmear()*Acts::UnitConstants::GeV);

            // Define perigee surface
            auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(Acts::Vector3(0,0,0));

            // Track particle back to PCA
            auto linesurface_parameter = -(vx*px + vy*py)/(px*px + py*py);
            auto xpca = vx + linesurface_parameter*px;
            auto ypca = vy + linesurface_parameter*py;
            auto zpca = vz + linesurface_parameter*pz;

            Acts::Vector3 global(xpca, ypca, zpca);
            Acts::Vector3 direction(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta));

            // Convert to local coordinates
            auto local = perigee->globalToLocal(
                m_service_geometry->GetActsGeometryContext(),
                global,
                direction
            );

            if(!local.ok()) {
                m_log->error("Skipping track: globalToLocal failed for track parameters");
                continue;
            }

            Acts::Vector2 localpos = local.value();
            double charge = 1;  // TODO: get from MC track

            // Create TrackParameters
            auto track_param = track_parameters->create();
            track_param.setType(-1); // seed
            track_param.setLoc({static_cast<float>(localpos(0)), static_cast<float>(localpos(1))});
            track_param.setPhi(phi);
            track_param.setTheta(theta);
            track_param.setQOverP(charge / pinit);
            track_param.setTime(mc_track.getHits().at(0).getTime());

            tdis::Cov6f cov;
            cov(0,0) = 1.0;   // loc0
            cov(1,1) = 1.0;   // loc1
            cov(2,2) = 0.05;  // phi
            cov(3,3) = 0.01;  // theta
            cov(4,4) = 0.1;   // qOverP
            cov(5,5) = 10e9;  // time
            track_param.setCovariance(cov);

            // -------------------------------
            // Step 3: Create TrackSeed
            // -------------------------------
            auto seed = seeds->create();

            // Set perigee (PCA point)
            seed.setPerigee({static_cast<float>(xpca),
                            static_cast<float>(ypca),
                            static_cast<float>(zpca)});

            // Add hits (use first N hits as seed hits, typically 3)
            int n_seed_hits = std::min(3, static_cast<int>(track_hits.size()));
            for (int i = 0; i < n_seed_hits; ++i) {
                seed.addToHits(track_hits[i]);
                if (i < track_measurements.size()) {
                    seed.addToMeasurements(track_measurements[i]);
                }
            }

            // Set track parameters
            seed.setParams(track_param);

            m_log->trace("Created seed with {} hits, perigee=({:.2f}, {:.2f}, {:.2f}), p={:.3f} GeV",
                       n_seed_hits, xpca, ypca, zpca, pinit);
        }

        // Move collections to output
        m_out_seeds() = std::move(seeds);
        m_out_trackerHits() = std::move(tracker_hits);
        m_out_measurements() = std::move(measurements);
        m_out_trackParams() = std::move(track_parameters);

        m_log->debug("Created {} seeds from MC tracks", m_out_seeds()->size());
    }
}
