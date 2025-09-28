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

    std::pair<std::optional<tdis::TrackerHit>, std::optional<tdis::Measurement2D>> 
    TruthTracksHitsSeedsFactory::createHitAndMeasurement(
        const tdis::DigitizedMtpcMcHit& mcHit,
        int plane,
        uint64_t event_index,
        const std::vector<double>& plane_positions) 
    {
        namespace ActsUnits = Acts::UnitConstants;

        // Basic geometry indices
        const int ring = mcHit.getRing();
        const int pad = mcHit.getPad();
        const double z_to_gem = mcHit.getZToGem();

        // Check for invalid pad values
        if (pad == -999 || pad == 999) {
            m_log->warn("At event #{} mc Hit had pad 999", event_index);
            return {std::nullopt, std::nullopt};
        }

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
        double onSurfaceTolerance = shouldUseTruePos ? maxPadDim : 1*Acts::UnitConstants::mm;

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
            return {std::nullopt, std::nullopt};
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

        return {hit, meas2D};
    }

    tdis::TrackSeed TruthTracksHitsSeedsFactory::createTrackSeedWithParameters(
        const tdis::DigitizedMtpcMcTrack& mc_track,
        const std::vector<tdis::TrackerHit>& trackHits,
        const std::vector<tdis::Measurement2D>& trackMeasurements)
    {
        namespace ActsUnits = Acts::UnitConstants;

        // Extract track kinematics from MC truth
        double momentum = mc_track.getMomentum();
        double theta = mc_track.getTheta();
        double phi = mc_track.getPhi();
        
        
        // Apply momentum smearing for more realistic seeding
        const double smearedMomentum = momentum * generateNormal(1.0, m_cfg_momentumSmear() * ActsUnits::GeV);
        
        // Use MC truth vertex position (collision point)
        // TDIS assumes vertex at (0, 0, vertexZ) along the beamline
        double vx = 0.0;
        double vy = 0.0;
        double vz = mc_track.getVertexZ();
        
        // Note: If vertex is not available or we want to use first hit for debugging,
        // uncomment the following:
        auto firstHit = mc_track.getHits().at(0).getTruePosition();
        vx = firstHit.x; vy = firstHit.y; vz = firstHit.z;
        
        // Momentum direction unit vector
        double px = std::sin(theta) * std::cos(phi);
        double py = std::sin(theta) * std::sin(phi);
        double pz = std::cos(theta);
        
        // Define perigee surface along beamline (Z-axis)
        // For TDIS, this is the standard reference line for track parameters
        auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(Acts::Vector3(0, 0, 0));
        
        // Calculate Point of Closest Approach (PCA) to the beamline
        // This finds where the track comes closest to the Z-axis
        // Parameter t along the track direction where distance to Z-axis is minimum
        double t = -(vx*px + vy*py)/(px*px + py*py);
        
        // PCA coordinates
        double xpca = vx + t*px;
        double ypca = vy + t*py;
        double zpca = vz + t*pz;
        
        // Convert PCA to local coordinates on perigee surface
        Acts::Vector3 globalPCA(xpca, ypca, zpca);
        Acts::Vector3 direction(px, py, pz);
        
        auto local = perigee->globalToLocal(
            m_service_geometry->GetActsGeometryContext(),
            globalPCA,
            direction
        );
        
        if (!local.ok()) {
            m_log->error("globalToLocal failed for track parameters at PCA");
            throw std::runtime_error("Failed to create track parameters");
        }
        
        Acts::Vector2 localpos = local.value();
        double charge = 1;  // TODO: get from MC track when available
        
        // Create TrackParameters at the perigee
        auto track_param = m_out_trackParams->create();
        track_param.setType(-1); // seed type
        track_param.setLoc({static_cast<float>(localpos(0)), static_cast<float>(localpos(1))});
        track_param.setPhi(phi);
        track_param.setTheta(theta);
        track_param.setQOverP(charge / smearedMomentum);
        track_param.setTime(mc_track.getHits().at(0).getTime());
        
        // Set covariance matrix for track parameters
        // Parameters are already variances in natural units
        tdis::Cov6f cov;
        cov(0,0) = m_cfg_covLoc0() * ActsUnits::mm * ActsUnits::mm;           // loc0 variance [mm^2]
        cov(1,1) = m_cfg_covLoc1() * ActsUnits::mm * ActsUnits::mm;           // loc1 variance [mm^2]
        cov(2,2) = m_cfg_covPhi();                                            // phi variance [rad^2]
        cov(3,3) = m_cfg_covTheta();                                          // theta variance [rad^2]
        cov(4,4) = m_cfg_covQOverP() / (ActsUnits::GeV * ActsUnits::GeV);     // q/p variance [(e/GeV)^2]
        cov(5,5) = m_cfg_covTime() * ActsUnits::ns * ActsUnits::ns;           // time variance [ns^2]
        track_param.setCovariance(cov);
        
        // Create TrackSeed
        auto seed = m_out_seeds()->create();
        
        // Set perigee point (PCA to beamline)
        seed.setPerigee({
            static_cast<float>(vx),
            static_cast<float>(vy),
            static_cast<float>(vz)
        });

        for (int i = 0; i < trackHits.size(); ++i) {
            seed.addToHits(trackHits[i]);
            if (i < trackMeasurements.size()) {
                seed.addToMeasurements(trackMeasurements[i]);
            }
        }
        
        // Set track parameters and MC track reference
        seed.setInitParams(track_param);
        seed.setMcTrack(mc_track);
        
        m_log->trace("Created seed with {} hits, vertex z={:.2f}, perigee PCA=({:.2f}, {:.2f}, {:.2f}), p={:.3f} GeV",
                   trackHits.size(), vz, xpca, ypca, zpca, smearedMomentum);
        
        return seed;
    }

    void TruthTracksHitsSeedsFactory::Execute(int32_t /*run_nr*/, uint64_t event_index) {
        namespace ActsUnits = Acts::UnitConstants;

        auto plane_positions = m_service_geometry->GetPlanePositions();

        m_log->trace("TruthTrackSeedFactory, processing event: {}", event_index);

        // Loop over MC tracks
        for (const auto& mc_track: *m_in_mcTracks()) {

            // Skip tracks without enough hits
            if (mc_track.hits_size() < m_cfg_minHitsForSeed()) {
                m_log->warn("Skipping track with {} hits (< minimum {})", 
                           mc_track.hits_size(), m_cfg_minHitsForSeed());
                continue;
            }

            // -------------------------------
            // Step 1: Create TrackerHits and Measurements from MC hits
            // -------------------------------
            std::vector<tdis::TrackerHit> trackHits;
            std::vector<tdis::Measurement2D> trackMeasurements;

            int plane_idx = 0;
            for (const auto& mcHit : mc_track.getHits()) {
                auto [hit_opt, meas_opt] = createHitAndMeasurement(
                    mcHit, plane_idx++, event_index, plane_positions);
                
                if (hit_opt.has_value() && meas_opt.has_value()) {
                    trackHits.push_back(hit_opt.value());
                    trackMeasurements.push_back(meas_opt.value());
                }
            }

            // Skip if we didn't create enough valid hits/measurements
            if (trackHits.size() < m_cfg_minHitsForSeed()) {
                m_log->debug("Skipping track with {} valid hits after processing", 
                           trackHits.size());
                continue;
            }

            // -------------------------------
            // Step 2: Create TrackSeed with TrackParameters
            // -------------------------------
            try {
                auto seed = createTrackSeedWithParameters(mc_track, trackHits, trackMeasurements);
            } catch (const std::exception& e) {
                m_log->error("Failed to create track seed: {}", e.what());
                continue;
            }
        }

        m_log->debug("Created {} seeds from MC tracks", m_out_seeds()->size());
    }
}
