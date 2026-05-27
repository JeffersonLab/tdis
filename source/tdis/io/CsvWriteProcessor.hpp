//
// Created by Dmitry Romanov on 9/24/2025.
// Subject to the terms in the LICENSE file found in the top-level directory.
//

#pragma once

#include <JANA/JEventProcessor.h>
#include <JANA/JObject.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

#include <Acts/Definitions/Units.hpp>
#include <fstream>
#include <vector>

#include "Acts/Definitions/Units.hpp"
#include "logging/LogService.hpp"
#include "podio_model/Measurement2D.h"
#include "podio_model/Track.h"
#include "podio_model/TrackCollection.h"
#include "podio_model/TrackParameters.h"
#include "podio_model/TrackSeed.h"
#include "podio_model/TrackSeedCollection.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/Trajectory.h"
#include "podio_model/TrajectoryCollection.h"

namespace tdis::io{
class CsvWriterProcessor : public JEventProcessor {

    // Input collections
    PodioInput<tdis::TrackSeed> m_in_trackSeeds {this, {"TruthTrackSeeds"}};
    PodioInput<tdis::Track> m_in_fittedTracks {this, {"FittedTracks"}};
    PodioInput<tdis::Trajectory> m_in_trajectories {this, {"FittedTrajectories"}};
    PodioInput<tdis::TrackParameters> m_in_trackParameters {this, {"FittedTrackParameters"}};

    Service<tdis::services::LogService> m_svc_log {this};

private:

    // taken from tdis:output parameter
    std::string m_cfg_filePrefix;

    std::string m_seedFileName;
    std::string m_hitFileName;
    std::string m_fittedTrackFileName;
    std::string m_trackStateFileName;

    std::ofstream m_seedFile;
    std::ofstream m_hitFile;
    std::ofstream m_fittedTrackFile;
    std::ofstream m_trackStateFile;

    // Logging
    std::shared_ptr<spdlog::logger> m_log;

    void writeSeedHeader() {
        fmt::print(m_seedFile,
            "evt,"              // 0 - event number/index
            "seed_id,"          // 1 - seed/track index
            "mc_mom,"           // 2 - MC total momentum
            "mc_phi,"           // 3 - MC phi angle at start
            "mc_theta,"         // 4 - MC theta angle at start
            "mc_vtx_z,"         // 5 - MC exact Z vertex
            "mc_hits_count,"    // 6 - MC hits count
            "pdg,"              // 7 - init truth particle PDG
            "init_phi,"         // 8 - init parameters phi
            "init_theta,"       // 9 - init parameters theta
            "init_time,"        // 10 - track time
            "init_qoverp,"      // 11 - q over p
            "init_surface,"     // 12 - surface ID
            "init_loc0,"        // 13 - location on surface 0
            "init_loc1,"        // 14 - location on surface 1
            "cov_loc0,"         // 15 - covariance loc0
            "cov_loc1,"         // 16 - covariance loc1
            "cov_phi,"          // 17 - covariance phi
            "cov_theta,"        // 18 - covariance theta
            "cov_qoverp,"       // 19 - covariance qoverp
            "cov_time,"         // 20 - covariance time
            "perigee_x,"        // 21 - perigee x
            "perigee_y,"        // 22 - perigee y
            "perigee_z,"        // 23 - perigee z
            "fhit_id,"          // 24 - first hit index
            "fhit_time,"        // 25 - first hit time
            "fhit_plane,"       // 26 - first hit plane
            "fhit_ring,"        // 27 - first hit ring
            "fhit_pad,"         // 28 - first hit pad
            "fhit_ztogem,"      // 29 - first hit z to gem
            "fhit_true_x,"      // 30 - first hit true x
            "fhit_true_y,"      // 31 - first hit true y
            "fhit_true_z"       // 32 - first hit true z
            "\n");
    }

    void writeHitHeader() {
        fmt::print(m_hitFile,
            "evt,"              // 0 - event number/index
            "seed_id,"          // 1 - seed/track index
            "meas_id,"          // 2 - measurement index in track
            "meas_time,"        // 3 - measurement time
            "meas_surface,"     // 4 - measurement surface ID
            "meas_loc0,"        // 5 - measurement local position 0
            "meas_loc1,"        // 6 - measurement local position 1
            "meas_cov0,"        // 7 - measurement covariance 0
            "meas_cov1,"        // 8 - measurement covariance 1
            "meas_cov_time,"    // 9 - measurement covariance time
            "hit_id,"           // 10 - tracker hit ID
            "hit_cell_id,"      // 11 - tracker hit cell ID
            "hit_x,"            // 12 - tracker hit x position
            "hit_y,"            // 13 - tracker hit y position
            "hit_z,"            // 14 - tracker hit z position
            "hit_time,"         // 15 - tracker hit time
            "hit_edep,"         // 16 - tracker hit energy deposit
            "mc_hit_id,"        // 17 - MC hit ID
            "mc_hit_plane,"     // 18 - MC hit plane
            "mc_hit_ring,"      // 19 - MC hit ring
            "mc_hit_pad,"       // 20 - MC hit pad
            "mc_hit_time,"      // 21 - MC hit time
            "mc_hit_adc,"       // 22 - MC hit ADC
            "mc_hit_ztogem,"    // 23 - MC hit z to gem
            "mc_hit_true_x,"    // 24 - MC hit true x
            "mc_hit_true_y,"    // 25 - MC hit true y
            "mc_hit_true_z"     // 26 - MC hit true z
            "\n");
    }

    void writeFittedTrackHeader() {
        fmt::print(m_fittedTrackFile,
            "evt,"              // 0 - event number
            "track_id,"         // 1 - fitted track ID
            "seed_id,"          // 2 - corresponding seed ID (from trajectory)
            "mc_p,"             // 3 - MC momentum [GeV]
            "mc_theta,"         // 4 - MC theta [rad]
            "mc_phi,"           // 5 - MC phi [rad]
            "mc_vtx_z,"         // 6 - MC vertex z [mm]
            "init_p,"           // 7 - initial momentum [GeV]
            "init_theta,"       // 8 - initial theta [rad]
            "init_phi,"         // 9 - initial phi [rad]
            "fit_p,"            // 10 - fitted momentum [GeV]
            "fit_theta,"        // 11 - fitted theta [rad]
            "fit_phi,"          // 12 - fitted phi [rad]
            "fit_vtx_z,"        // 13 - fitted vertex z [mm]
            "fit_chi2ndf,"      // 14 - chi2/ndf
            "fit_type,"         // 15 - track type
            "fit_px,"           // 16 - fitted momentum x [GeV]
            "fit_py,"           // 17 - fitted momentum y [GeV]
            "fit_pz,"           // 18 - fitted momentum z [GeV]
            "fit_vtx_x,"        // 19 - fitted vertex x [mm]
            "fit_vtx_y,"        // 20 - fitted vertex y [mm]
            "fit_time,"         // 21 - fitted time [ns]
            "fit_time_err,"     // 22 - fitted time error
            "fit_charge,"       // 23 - fitted charge
            "fit_chi2,"         // 24 - chi2
            "fit_ndf,"          // 25 - number of degrees of freedom
            "fit_pdg,"          // 26 - PDG hypothesis
            "n_states,"         // 27 - number of track states
            "n_measurements,"   // 28 - number of measurements
            "n_outliers,"       // 29 - number of outliers
            "n_holes"           // 30 - number of holes
            "\n");
    }

    void writeTrackStateHeader() {
        fmt::print(m_trackStateFile,
            "evt,"              // 0 - event number
            "track_id,"         // 1 - fitted track ID
            "seed_id,"          // 2 - corresponding seed ID
            "state_idx,"        // 3 - state index
            "surface_id,"       // 4 - surface ID
            "loc0,"             // 5 - local position 0
            "loc1,"             // 6 - local position 1
            "phi,"              // 7 - phi
            "theta,"            // 8 - theta
            "qoverp,"           // 9 - q/p
            "p,"                // 10 - momentum [GeV]
            "time,"             // 11 - time
            "path_length,"      // 12 - path length
            "chi2,"             // 13 - state chi2
            "type"              // 14 - state type (measurement, outlier, hole)
            "\n");
    }

public:

    CsvWriterProcessor() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
    }

    void Init() override {

        // Initialize logger
        m_log = m_svc_log->logger("CsvWriteProcessor");

        // Get global output prefix name
        m_cfg_filePrefix = GetApplication()->GetParameterValue<std::string>("tdis:output");

        // Construct file names
        m_seedFileName = m_cfg_filePrefix + ".seeds.csv";
        m_hitFileName = m_cfg_filePrefix + ".hits.csv";
        m_fittedTrackFileName = m_cfg_filePrefix + ".fitted_tracks.csv";
        m_trackStateFileName = m_cfg_filePrefix + ".track_states.csv";

        m_log->info("Seed output file: {}", m_seedFileName);
        m_log->info("Hit output file: {}", m_hitFileName);
        m_log->info("Fitted track output file: {}", m_fittedTrackFileName);
        m_log->info("Track state output file: {}", m_trackStateFileName);
        m_log->info("opening files...");

        // Open files
        m_seedFile.open(m_seedFileName);
        m_hitFile.open(m_hitFileName);
        m_fittedTrackFile.open(m_fittedTrackFileName);
        m_trackStateFile.open(m_trackStateFileName);

        if (!m_seedFile.is_open()) {
            throw JException("Failed to open seed output file: %s", m_seedFileName.c_str());
        }
        if (!m_hitFile.is_open()) {
            throw JException("Failed to open hit output file: %s", m_hitFileName.c_str());
        }
        if (!m_fittedTrackFile.is_open()) {
            throw JException("Failed to open fitted track output file: %s", m_fittedTrackFileName.c_str());
        }
        if (!m_trackStateFile.is_open()) {
            throw JException("Failed to open track state output file: %s", m_trackStateFileName.c_str());
        }

        writeSeedHeader();
        writeHitHeader();
        writeFittedTrackHeader();
        writeTrackStateHeader();

        m_log->info("All files are opened and ready for recording...");
    }

    void ProcessSequential(const JEvent& event) override {
        uint64_t eventNumber = event.GetEventNumber();

        // Process track seeds (input)
        auto& seeds = *m_in_trackSeeds();
        m_log->debug("Processing event {} with {} seeds", eventNumber, seeds.size());

        for (auto trueSeed: seeds) {
            auto mcTrack = trueSeed.getMcTrack();
            if (!mcTrack.isAvailable()) {
                auto message = fmt::format("!trueSeed.getMcTrack().isAvailable() ERROR in data composition");
                throw JException(message);
            }

            // Write seed info
            writeSeed(eventNumber, trueSeed);

            // Write measurements/hits for this seed
            auto measurements = trueSeed.getMeasurements();
            for (size_t measIdx = 0; measIdx < measurements.size(); ++measIdx) {
                writeMeasurement(eventNumber, trueSeed, measurements[measIdx], measIdx);
            }
        }

        // Process fitted tracks (output)
        try {
            auto& tracks = *m_in_fittedTracks();
            auto& trajectories = *m_in_trajectories();
            auto& trackParameters = *m_in_trackParameters();

            m_log->debug("Processing {} fitted tracks", tracks.size());

            for (auto track : tracks) {
                writeFittedTrack(eventNumber, track, seeds);

                // Write track states if trajectory is available
                auto trajectory = track.getTrajectory();
                if (trajectory.isAvailable()) {
                    writeTrackStates(eventNumber, track, trajectory);
                }
            }
        } catch (const std::exception& e) {
            m_log->warn("Failed to process fitted tracks for event {}: {}", eventNumber, e.what());
        }
    }

    void writeSeed(uint64_t eventIndex, const TrackSeed& seed) {
        auto mcTrack = seed.getMcTrack();
        auto params = seed.getInitParams();
        auto trkCov = params.getCovariance();
        auto loc = params.getLoc();
        auto perigee = seed.getPerigee();

        fmt::print(m_seedFile, "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
            eventIndex,                 // 0 - evt
            seed.getObjectID().index,   // 1 - seed_id
            mcTrack.getMomentum(),      // 2 - mc_mom
            mcTrack.getPhi(),           // 3 - mc_phi
            mcTrack.getTheta(),         // 4 - mc_theta
            mcTrack.getVertexZ(),       // 5 - mc_vtx_z
            mcTrack.getHits().size(),   // 6 - mc_hits_count
            params.getPdg(),            // 7 - pdg
            params.getPhi(),            // 8 - init_phi
            params.getTheta(),          // 9 - init_theta
            params.getTime(),           // 10 - init_time
            params.getQOverP(),         // 11 - init_qoverp
            params.getSurface(),        // 12 - init_surface
            loc[0],                     // 13 - init_loc0
            loc[1],                     // 14 - init_loc1
            trkCov(0,0),                // 15 - cov_loc0
            trkCov(1,1),                // 16 - cov_loc1
            trkCov(2,2),                // 17 - cov_phi
            trkCov(3,3),                // 18 - cov_theta
            trkCov(4,4),                // 19 - cov_qoverp
            trkCov(5,5),                // 20 - cov_time
            perigee.x,                  // 21 - perigee_x
            perigee.y,                  // 22 - perigee_y
            perigee.z                   // 23 - perigee_z
        );

        // First hit info
        if (!mcTrack.getHits().empty()) {
            auto firstHit = mcTrack.getHits()[0];
            fmt::print(m_seedFile, ",{},{},{},{},{},{},{},{},{}",
                firstHit.getObjectID().index,
                firstHit.getTime(),
                firstHit.getPlane(),
                firstHit.getRing(),
                firstHit.getPad(),
                firstHit.getZToGem(),
                firstHit.getTruePosition().x,
                firstHit.getTruePosition().y,
                firstHit.getTruePosition().z
            );
        } else {
            fmt::print(m_seedFile, ",,,,,,,,");
        }
        fmt::print(m_seedFile, "\n");
    }

    void writeMeasurement(uint64_t eventIndex, const TrackSeed& seed, const Measurement2D& measurement, size_t measIdx) {
        auto cov = measurement.getCovariance();
        auto loc = measurement.getLoc();
        auto trackerHit = measurement.getHits().at(0);
        auto mcHit = trackerHit.getRawHit();
        auto hitPos = trackerHit.getPosition();

        fmt::print(m_hitFile, "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
            eventIndex,                     // 0 - evt
            seed.getObjectID().index,       // 1 - seed_id
            measIdx,                        // 2 - meas_id
            measurement.getTime(),          // 3 - meas_time
            measurement.getSurface(),       // 4 - meas_surface
            loc[0],                         // 5 - meas_loc0
            loc[1],                         // 6 - meas_loc1
            cov(0,0),                       // 7 - meas_cov0
            cov(1,1),                       // 8 - meas_cov1
            cov(2,2),                       // 9 - meas_cov_time
            trackerHit.getObjectID().index, // 10 - hit_id
            trackerHit.getCellID(),         // 11 - hit_cell_id
            hitPos.x,                       // 12 - hit_x
            hitPos.y,                       // 13 - hit_y
            hitPos.z,                       // 14 - hit_z
            trackerHit.getTime(),           // 15 - hit_time
            trackerHit.getEdep(),           // 16 - hit_edep
            mcHit.getObjectID().index,      // 17 - mc_hit_id
            mcHit.getPlane(),               // 18 - mc_hit_plane
            mcHit.getRing(),                // 19 - mc_hit_ring
            mcHit.getPad(),                 // 20 - mc_hit_pad
            mcHit.getTime(),                // 21 - mc_hit_time
            mcHit.getAdc(),                 // 22 - mc_hit_adc
            mcHit.getZToGem(),              // 23 - mc_hit_ztogem
            mcHit.getTruePosition().x,      // 24 - mc_hit_true_x
            mcHit.getTruePosition().y,      // 25 - mc_hit_true_y
            mcHit.getTruePosition().z       // 26 - mc_hit_true_z
        );
        fmt::print(m_hitFile, "\n");
    }

    void writeFittedTrack(uint64_t eventIndex, const Track& track, const tdis::TrackSeedCollection& seeds) {
        auto trajectory = track.getTrajectory();
        auto momentum = track.getMomentum();
        auto position = track.getPosition();

        // Calculate fitted momentum and angles
        double fit_p = std::sqrt(momentum.x*momentum.x + momentum.y*momentum.y + momentum.z*momentum.z);
        double fit_theta = (fit_p > 0) ? std::acos(momentum.z / fit_p) : 0;
        double fit_phi = std::atan2(momentum.y, momentum.x);

        // Get seed ID and find corresponding seed for MC and initial parameters
        int seedId = -1;
        double mc_p = -999, mc_theta = -999, mc_phi = -999, mc_vtx_z = -999;
        double init_p = -999, init_theta = -999, init_phi = -999;

        if (trajectory.isAvailable() && trajectory.getSeed().isAvailable()) {
            auto seed = trajectory.getSeed();
            seedId = seed.getObjectID().index;

            // Get MC truth from seed
            if (seed.getMcTrack().isAvailable()) {
                auto mcTrack = seed.getMcTrack();
                mc_p = mcTrack.getMomentum();
                mc_theta = mcTrack.getTheta();
                mc_phi = mcTrack.getPhi();
                mc_vtx_z = mcTrack.getVertexZ();
            }

            // Get initial parameters from seed
            if (seed.getInitParams().isAvailable()) {
                auto initParams = seed.getInitParams();
                init_p = (initParams.getQOverP() != 0) ? std::abs(1.0 / initParams.getQOverP()) : -999;
                init_theta = initParams.getTheta();
                init_phi = initParams.getPhi();
            }
        } else {
            // Try to find seed by matching in the collection
            for (auto s : seeds) {
                if (s.getObjectID().index == seedId) {
                    if (s.getMcTrack().isAvailable()) {
                        auto mcTrack = s.getMcTrack();
                        mc_p = mcTrack.getMomentum();
                        mc_theta = mcTrack.getTheta();
                        mc_phi = mcTrack.getPhi();
                        mc_vtx_z = mcTrack.getVertexZ();
                    }
                    if (s.getInitParams().isAvailable()) {
                        auto initParams = s.getInitParams();
                        init_p = (initParams.getQOverP() != 0) ? std::abs(1.0 / initParams.getQOverP()) : -999;
                        init_theta = initParams.getTheta();
                        init_phi = initParams.getPhi();
                    }
                    break;
                }
            }
        }

        double fit_chi2ndf = (track.getNdf() > 0) ? track.getChi2()/track.getNdf() : -1;

        fmt::print(m_fittedTrackFile, "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
            eventIndex,                         // 0 - evt
            track.getObjectID().index,          // 1 - track_id
            seedId,                             // 2 - seed_id
            mc_p,                               // 3 - mc_p
            mc_theta,                           // 4 - mc_theta
            mc_phi,                             // 5 - mc_phi
            mc_vtx_z,                           // 6 - mc_vtx_z
            init_p,                             // 7 - init_p
            init_theta,                         // 8 - init_theta
            init_phi,                           // 9 - init_phi
            fit_p,                              // 10 - fit_p
            fit_theta,                          // 11 - fit_theta
            fit_phi,                            // 12 - fit_phi
            position.z,                         // 13 - fit_vtx_z
            fit_chi2ndf,                        // 14 - fit_chi2ndf
            track.getType(),                    // 15 - fit_type
            momentum.x,                         // 16 - fit_px
            momentum.y,                         // 17 - fit_py
            momentum.z,                         // 18 - fit_pz
            position.x,                         // 19 - fit_vtx_x
            position.y,                         // 20 - fit_vtx_y
            track.getTime(),                    // 21 - fit_time
            track.getTimeError(),               // 22 - fit_time_err
            track.getCharge(),                  // 23 - fit_charge
            track.getChi2(),                    // 24 - fit_chi2
            track.getNdf(),                     // 25 - fit_ndf
            track.getPdg(),                     // 26 - fit_pdg
            trajectory.isAvailable() ? trajectory.getNStates() : 0,           // 27 - n_states
            trajectory.isAvailable() ? trajectory.getNMeasurements() : 0,     // 28 - n_measurements
            trajectory.isAvailable() ? trajectory.getNOutliers() : 0,         // 29 - n_outliers
            trajectory.isAvailable() ? trajectory.getNHoles() : 0             // 30 - n_holes
        );
        fmt::print(m_fittedTrackFile, "\n");
    }

    void writeTrackStates(uint64_t eventIndex, const Track& track, const Trajectory& trajectory) {
        auto trackParams = trajectory.getTrackParameters();

        // Get seed ID
        int seedId = -1;
        if (trajectory.getSeed().isAvailable()) {
            seedId = trajectory.getSeed().getObjectID().index;
        }

        for (size_t stateIdx = 0; stateIdx < trackParams.size(); ++stateIdx) {
            auto params = trackParams[stateIdx];
            auto loc = params.getLoc();

            // Calculate momentum from q/p
            double qOverP = params.getQOverP();
            double p = (qOverP != 0) ? std::abs(1.0 / qOverP) : 0;

            // Determine state type (simplified - you may need more logic here)
            std::string stateType = "measurement";
            if (stateIdx < trajectory.getOutlierChi2().size() && trajectory.getOutlierChi2()[stateIdx] > 0) {
                stateType = "outlier";
            }

            fmt::print(m_trackStateFile, "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
                eventIndex,                     // 0 - evt
                track.getObjectID().index,      // 1 - track_id
                seedId,                         // 2 - seed_id
                stateIdx,                       // 3 - state_idx
                params.getSurface(),            // 4 - surface_id
                loc[0],                         // 5 - loc0
                loc[1],                         // 6 - loc1
                params.getPhi(),                // 7 - phi
                params.getTheta(),              // 8 - theta
                qOverP,                         // 9 - qoverp
                p,                              // 10 - p
                params.getTime(),               // 11 - time
                0.0,                            // 12 - path_length (not stored, placeholder)
                stateIdx < trajectory.getMeasurementChi2().size() ? trajectory.getMeasurementChi2()[stateIdx] : 0, // 13 - chi2
                stateType                       // 14 - type
            );
            fmt::print(m_trackStateFile, "\n");
        }
    }

    void Finish() override {
        if (m_seedFile.is_open()) {
            m_seedFile.close();
            m_log->info("Closed seed output file: {}", m_seedFileName);
        }
        if (m_hitFile.is_open()) {
            m_hitFile.close();
            m_log->info("Closed hit output file: {}", m_hitFileName);
        }
        if (m_fittedTrackFile.is_open()) {
            m_fittedTrackFile.close();
            m_log->info("Closed fitted track output file: {}", m_fittedTrackFileName);
        }
        if (m_trackStateFile.is_open()) {
            m_trackStateFile.close();
            m_log->info("Closed track state output file: {}", m_trackStateFileName);
        }
    }
};

} // namespace tdis::io
