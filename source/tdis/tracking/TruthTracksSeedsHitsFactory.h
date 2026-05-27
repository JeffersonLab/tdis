#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

#include <optional>
#include <random>

#include "geometry/ActsGeometryService.h"
#include "geometry/TGeoGeometryService.h"
#include "podio_model/DigitizedMtpcMcHit.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/TrackParameters.h"
#include "podio_model/TrackSeed.h"
#include "podio_model/TrackerHit.h"

namespace tdis::tracking {

    struct TruthTracksSeedsHitsFactory : public JOmniFactory<TruthTracksSeedsHitsFactory> {

        // -----------------------------------------------------------------------
        // Input collections
        // -----------------------------------------------------------------------
        PodioInput<tdis::DigitizedMtpcMcTrack> m_in_mcTracks {this, {"DigitizedMtpcMcTracks"}};

        // -----------------------------------------------------------------------
        // Output collections
        // -----------------------------------------------------------------------
        PodioOutput<tdis::TrackSeed>        m_out_seeds       {this, "TruthTrackSeeds"};
        PodioOutput<tdis::TrackParameters>  m_out_trackParams {this, "TruthTrackParameters"};
        PodioOutput<tdis::TrackerHit>       m_out_trackerHits {this, "TrackerHits"};
        PodioOutput<tdis::Measurement2D>    m_out_measurements{this, "Measurements2D"};

        // -----------------------------------------------------------------------
        // Services
        // -----------------------------------------------------------------------
        Service<ActsGeometryService>       m_service_geometry{this};
        Service<services::LogService>      m_service_log     {this};

        // -----------------------------------------------------------------------
        // Parameters
        // -----------------------------------------------------------------------

        /// When true, use the MC true hit position instead of the digitized
        /// pad-centre position. Falls back automatically when true_x is NaN.
        Parameter<bool> m_cfg_useTrueHitPos{this,
            "acts:use_true_hit_position",
            true,
            "Use true hits xyz instead of digitized one"
        };

        /// Fractional (dimensionless) 1-sigma Gaussian smear applied to the
        /// seed momentum.  E.g. 0.1 means 10 % smear.  Must NOT be multiplied
        /// by any unit constant.
        Parameter<double> m_cfg_momentumSmear{this,
            "acts:track_init:momentum_smear",
            0.1,
            "Fractional (dimensionless) 1-sigma Gaussian smear on seed momentum"
        };

        Parameter<int> m_cfg_minHitsForSeed{this,
            "acts:seed:min_hits",
            3,
            "Minimum number of valid hits required to create a seed"
        };

        Parameter<int> m_cfg_maxHitsForSeed{this,
            "acts:seed:max_hits",
            3,
            "Maximum number of hits to include in a seed (typically 3-5)"
        };

        /// When true, override the MC vertex position with the first hit's true
        /// position.  Useful for debugging, but should be false in production.
        Parameter<bool> m_cfg_useFirstHitAsVertex{this,
            "acts:seed:use_first_hit_as_vertex",
            false,
            "Override MC vertex position with first-hit true position (debug only)"
        };

        // -----------------------------------------------------------------------
        // Covariance parameters for initial track parameters (all are VARIANCES,
        // i.e. squared uncertainties, in the units indicated in the description).
        // -----------------------------------------------------------------------
        Parameter<double> m_cfg_covLoc0{this,
            "acts:seed:cov:loc0",
            1.0,
            "[mm^2] Variance for local position 0 (transverse impact parameter)"
        };
        Parameter<double> m_cfg_covLoc1{this,
            "acts:seed:cov:loc1",
            1.0,
            "[mm^2] Variance for local position 1 (z at perigee)"
        };
        Parameter<double> m_cfg_covPhi{this,
            "acts:seed:cov:phi",
            0.05,
            "[rad^2] Variance for phi angle"
        };
        Parameter<double> m_cfg_covTheta{this,
            "acts:seed:cov:theta",
            0.01,
            "[rad^2] Variance for theta angle"
        };
        Parameter<double> m_cfg_covQOverP{this,
            "acts:seed:cov:qoverp",
            0.1,
            "[(e/GeV)^2] Variance for charge over momentum"
        };
        Parameter<double> m_cfg_covTime{this,
            "acts:seed:cov:time",
            1.0e10,
            "[ns^2] Variance for time"
        };

        // -----------------------------------------------------------------------
        // Internal state
        // -----------------------------------------------------------------------
        std::shared_ptr<spdlog::logger> m_log;
        std::mt19937 m_generator;

        // -----------------------------------------------------------------------
        // JANA2 callbacks
        // -----------------------------------------------------------------------
        void Configure();
        void Execute(int32_t run_nr, uint64_t event_index);

    private:
        /// Returns a sample from N(mean, stddev).
        double generateNormal(double mean, double stddev);

        /**
         * @brief Creates one TrackerHit and one Measurement2D from a single MC hit.
         *
         * Position is taken from the true hit XYZ when m_cfg_useTrueHitPos is set
         * and the true coordinates are finite; otherwise the digitised pad-centre
         * is used.  The hit is skipped (nullopt returned for both outputs) when
         * the pad index is sentinel ±999 or when the globalToLocal projection
         * fails.
         *
         * @param mcHit          Source MC hit.
         * @param plane          Plane index within the event.
         * @param event_index    Event counter (for log messages).
         * @param plane_positions  Vector of nominal Z positions for each plane [mm].
         * @return {TrackerHit, Measurement2D} — either both valid or both nullopt.
         */
        std::pair<std::optional<tdis::TrackerHit>, std::optional<tdis::Measurement2D>>
        createHitAndMeasurement(
            const tdis::DigitizedMtpcMcHit& mcHit,
            int                             plane,
            uint64_t                        event_index,
            const std::vector<double>&      plane_positions);

        /**
         * @brief Creates a TrackSeed with embedded TrackParameters from MC truth.
         *
         * Momentum is smeared by a fractional Gaussian whose width is given by
         * m_cfg_momentumSmear (dimensionless).  The perigee surface is the
         * Z-axis; track parameters are computed at the Point of Closest Approach
         * (PCA) to that axis.
         *
         * @param mc_track          Source MC track (provides p, θ, φ, vertex).
         * @param trackHits         Pre-built TrackerHits for this track.
         * @param trackMeasurements Pre-built Measurement2Ds for this track.
         * @return The new TrackSeed (already inserted into m_out_seeds).
         * @throws std::runtime_error when the perigee globalToLocal conversion fails.
         */
        tdis::TrackSeed createTrackSeedWithParameters(
            const tdis::DigitizedMtpcMcTrack&       mc_track,
            const std::vector<tdis::TrackerHit>&    trackHits,
            const std::vector<tdis::Measurement2D>& trackMeasurements);
    };

} // namespace tdis::tracking
