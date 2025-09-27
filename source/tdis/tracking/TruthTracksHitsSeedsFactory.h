#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>
#include <random>

#include "ActsGeometryService.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
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

        std::shared_ptr<spdlog::logger> m_log;
        std::mt19937 m_generator;

        void Configure();
        void Execute(int32_t run_nr, uint64_t event_index);
        
    private:
        double generateNormal(double mean, double stddev);
    };
}
