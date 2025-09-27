#include "TruthTracksHitsSeedsFactory.h"

#include <podio_model/DigitizedMtpcMcTrack.h>
#include <podio_model/DigitizedMtpcMcTrackCollection.h>

#include <Acts/Definitions/Units.hpp>
#include <Acts/Surfaces/CylinderBounds.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>

#include "PadGeometryHelper.hpp"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrackSeedCollection.h"
#include "podio_model/TrackerHitCollection.h"

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
        m_log = m_service_log->logger("TruthTracksHitsSeedsFactory");

        // Initialize random generator
        std::random_device rd;
        m_generator = std::mt19937(rd());
    }

    double TruthTracksHitsSeedsFactory::generateNormal(double mean, double stddev) {
        std::normal_distribution<double> distribution(mean, stddev);
        return distribution(m_generator);
    }

    void TruthTracksHitsSeedsFactory::Execute(int32_t /*run_nr*/, uint64_t event_index) {
        namespace ActsUnits = Acts::UnitConstants;

        // Create output collections
        // auto seeds = std::make_unique<tdis::TrackSeedCollection>();
        // auto tracker_hits = std::make_unique<tdis::TrackerHitCollection>();
        // auto measurements = std::make_unique<tdis::Measurement2DCollection>();
        // auto track_parameters = std::make_unique<tdis::TrackParametersCollection>();

        auto plane_positions = m_service_geometry->GetPlanePositions();

        m_log->trace("TruthTrackSeedFactory, processing event: {}", event_index);

        // Loop over MC tracks
        for (const auto& mc_track: *m_in_mcTracks()) {

            // Skip tracks without enough hits
            if(mc_track.hits_size() < m_cfg_minHitsForSeed()) {
                m_log->warn("Skipping track with {} hits (< minimum {})", mc_track.hits_size(), m_cfg_minHitsForSeed());
                continue;
            }

            // -------------------------------
            // Step 1: Create TrackerHits and Measurements from MC hits
            // -------------------------------
            std::vector<tdis::TrackerHit> trackHits;
            std::vector<tdis::Measurement2D> trackMeasurements;

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

                // Flag if true position should be used
                // Input files may not provide the true position of hits
                bool shouldUseTruePos = m_cfg_useTrueHitPos() && !std::isnan(mcHit.getTruePosition().x);

                // Choose position: either true or digitized
                Vector3f position;
                if (shouldUseTruePos) {
                    position = mcHit.getTruePosition();
                } else {
                    position.x = static_cast<float>(padX);
                    position.y = static_cast<float>(padY);
                    position.z = static_cast<float>(hitCalcZ);
                }

                // Covariance estimate
                double maxPadDim = std::max(getPadApproxWidth(ring), getPadHight());
                double xy_variance = get_variance(maxPadDim);
                tdis::CovDiag3f cov{
                    static_cast<float>(xy_variance),
                    static_cast<float>(xy_variance),
                    static_cast<float>(1*ActsUnits::cm)
                };

                uint32_t cell_id = 1'000'000 * plane + 1'000 * ring + pad;

                // Create TrackerHit
                auto hit = m_out_trackerHits()->create();
                hit.setTime(cell_id);
                hit.setPosition(position);
                hit.setPositionError(cov);
                hit.setTime(cell_id);
                hit.setTimeError(1*ActsUnits::ms);   // Time resolution
                hit.setEdep(mcHit.getAdc());
                hit.setEdepError(0.0F);
                hit.setRawHit(mcHit);

                trackHits.push_back(hit);

                // Create Measurement2D
                auto actsDetElement = m_service_geometry().GetDetectorCylinder(ring);
                auto& surfaceRef = actsDetElement->surface();
                auto geometryId = surfaceRef.geometryId().value();

                // Convert to local coordinates
                Acts::Vector2 loc = Acts::Vector2::Zero();

                // If pad centers are used for hit position,
                // pad centers are always lay on cylinders so we take small tolerance
                // But if we take true positions they might be off from surface in pad dims
                // This tolerance is used in globalToLocal conversion
                double onSurfaceTolerance = shouldUseTruePos? maxPadDim : 1*Acts::UnitConstants::mm;

                try {
                    Acts::Vector2 pos = surfaceRef
                        .globalToLocal(
                            Acts::GeometryContext(),
                            {position.x, position.y, position.z},
                            Acts::Vector3::Zero(),
                            onSurfaceTolerance
                        ).value();

                    loc[Acts::eBoundLoc0] = pos[0];
                    loc[Acts::eBoundLoc1] = pos[1];

                } catch (std::exception& ex) {
                    m_log->warn("Can't convert globalToLocal for hit: plane={} ring={} pad={},"
                                " onSurfaceTolerance={}, error: '{}' SKIPPING HIT",
                               plane, ring, pad, onSurfaceTolerance, ex.what());
                    continue;
                }

                // Create measurement
                auto meas2D = m_out_measurements()->create();
                meas2D.setSurface(geometryId);
                meas2D.setLoc({static_cast<float>(loc[0]), static_cast<float>(loc[1])});
                meas2D.setTime(hit.getTime());
                meas2D.setCovariance({
                    cov(0, 0),
                    cov(1, 1),
                    hit.getTimeError() * hit.getTimeError(),
                });
                meas2D.addToWeights(1.0);
                meas2D.addToHits(hit);
                trackMeasurements.push_back(meas2D);
            }

            // Skip if we didn't create enough valid hits/measurements
            if (trackHits.size() < m_cfg_minHitsForSeed()) {
                m_log->debug("Skipping track with {} valid hits after processing", trackHits.size());
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
            auto track_param = m_out_trackParams->create();
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
            cov(5,5) = 10e6;  // time
            track_param.setCovariance(cov);

            // -------------------------------
            // Step 3: Create TrackSeed
            // -------------------------------
            auto seed = m_out_seeds()->create();

            // Set perigee (PCA point)
            seed.setPerigee({static_cast<float>(xpca),
                            static_cast<float>(ypca),
                            static_cast<float>(zpca)});

            // Add hits (use first N hits as seed hits, typically 3)
            int n_seed_hits = std::min(3, static_cast<int>(trackHits.size()));
            for (int i = 0; i < n_seed_hits; ++i) {
                seed.addToHits(trackHits[i]);
                if (i < trackMeasurements.size()) {
                    seed.addToMeasurements(trackMeasurements[i]);
                }
            }

            // Set track parameters
            seed.setInitParams(track_param);
            seed.setMcTrack(mc_track);

            m_log->trace("Created seed with {} hits, perigee=({:.2f}, {:.2f}, {:.2f}), p={:.3f} GeV",
                       n_seed_hits, xpca, ypca, zpca, pinit);
        }

        // Move collections to output
        // m_out_seeds() = std::move(seeds);
        // m_out_trackerHits() = std::move(tracker_hits);
        // m_out_measurements() = std::move(measurements);
        // m_out_trackParams() = std::move(track_parameters);

        m_log->debug("Created {} seeds from MC tracks", m_out_seeds()->size());
    }
}
