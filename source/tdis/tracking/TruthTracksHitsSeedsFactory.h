#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>
#include <random>
#include <optional>

#include "ActsGeometryService.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/DigitizedMtpcMcHit.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/TrackParameters.h"
#include "podio_model/TrackSeed.h"
#include "podio_model/TrackerHit.h"


namespace tdis::tracking {

    struct TruthTracksHitsSeedsFactory : public JOmniFactory<TruthTracksHitsSeedsFactory> {
        // Input
        PodioInput<tdis::DigitizedMtpcMcTrack> m_in_mcTracks {this, {"DigitizedMtpcMcTracks"}};

        // Outputs
        PodioOutput<tdis::TrackSeed>        m_out_seeds {this, "TruthTrackSeeds"};
        PodioOutput<tdis::TrackParameters>  m_out_trackParams {this, "TruthTrackParameters"};
        PodioOutput<tdis::TrackerHit>       m_out_trackerHits {this, "TrackerHits"};
        PodioOutput<tdis::Measurement2D>    m_out_measurements {this, "Measurements2D"};

        // Services
        Service<ActsGeometryService> m_service_geometry{this};
        Service<services::LogService> m_service_log{this};

        // Parameters
        Parameter<bool> m_cfg_useTrueHitPos{this,
            "acts:use_true_hit_position",
            true,
            "Use true hits xyz instead of digitized one"
        };
        Parameter<double> m_cfg_momentumSmear{this,
            "acts:track_init:momentum_smear",
            0.1,
            "[GeV], Momentum smear for truth track initialization"
        };
        Parameter<int> m_cfg_minHitsForSeed{this,
            "acts:seed:min_hits",
            3,
            "Minimum number of hits to create a seed"
        };
        Parameter<int> m_cfg_maxHitsForSeed{this,
            "acts:seed:max_hits",
            20,
            "Maximum number of hits to include in a seed"
        };

        std::shared_ptr<spdlog::logger> m_log;
        std::mt19937 m_generator;

        void Configure();
        void Execute(int32_t run_nr, uint64_t event_index);
        
    private:
        double generateNormal(double mean, double stddev);
        
        /**
         * Creates both TrackerHit and Measurement2D from a single MC hit
         * Combines common logic for both object creation
         * @return pair of optional TrackerHit and Measurement2D (nullopt if creation failed)
         */
        std::pair<std::optional<tdis::TrackerHit>, std::optional<tdis::Measurement2D>> 
        createHitAndMeasurement(
            const tdis::DigitizedMtpcMcHit& mcHit,
            int plane,
            uint64_t event_index,
            const std::vector<double>& plane_positions);
        
        /**
         * Creates TrackSeed with embedded TrackParameters from MC track information
         * Handles momentum smearing and track parameter creation
         * Calculates Point of Closest Approach (PCA) to beamline for proper perigee parameters
         * @param mc_track The MC track to process
         * @param trackHits Vector of hits associated with this track
         * @param trackMeasurements Vector of measurements associated with this track
         * @return Created TrackSeed with initialized parameters
         * @throws std::runtime_error if track parameter creation fails
         */
        tdis::TrackSeed createTrackSeedWithParameters(
            const tdis::DigitizedMtpcMcTrack& mc_track,
            const std::vector<tdis::TrackerHit>& trackHits,
            const std::vector<tdis::Measurement2D>& trackMeasurements);
    };
}
