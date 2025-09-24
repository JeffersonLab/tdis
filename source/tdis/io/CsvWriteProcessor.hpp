//
// Created by Dmitry Romanov on 9/24/2025.
// Subject to the terms in the LICENSE file found in the top-level directory.
//

#pragma once

// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#include <JANA/JEventProcessor.h>
#include <JANA/JObject.h>
#include <Acts/Definitions/Units.hpp>

#include <vector>
#include <fstream>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

#include "podio_model/TrackSeed.h"
#include "podio_model/TrackSeedCollection.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/TrackParameters.h"
#include "services/LogService.hpp"
#include "Acts/Definitions/Units.hpp"

namespace tdis::io{
class CsvWriterProcessor : public JEventProcessor {


    // Parameters following m_cfg_xxxYyy convention
    Parameter<std::string> m_cfg_filePrefix {this,
        "csv:prefix",
        "output",
        "File name prefix with path for CSV output files"};

    PodioInput<tdis::TrackSeed> m_in_trackSeeds {this, {"TruthTrackSeeds"}};
    Service<tdis::services::LogService> m_svc_log {this};

private:
    // Input collection type/tag pairs following m_in_xxxYyy convention
    std::pair<std::string, std::string> m_in_trackSeedTypeTag;
    std::pair<std::string, std::string> m_in_trackerHitTypeTag;

    // Member variables following m_xxxYyyy convention
    std::string m_trackFileName;
    std::string m_hitFileName;
    std::ofstream m_trackFile;
    std::ofstream m_hitFile;


    // Logging
    std::shared_ptr<spdlog::logger> m_log;


    void writeTrackHeader() {
        fmt::print(m_trackFile,
            "evt,"              // 0 - event number/index
            "trk_id,"           // 1 - track index
            "mc_mom,"           // 2 - MC total momentum
            "mc_phi,"           // 3 - MC phi angle at start
            "mc_theta,"         // 4 - MC theta angle at start
            "mc_vtx_z,"         // 5 - MC exact Z vertex
            "mc_hits_count,"    // 6 - MC hits count
            "pdg,"              // 7 - init truth particle PDG
            "tp_phi,"           // 8 - init truth parameters phi
            "tp_theta,"         // 9 - init truth parameters theta
            "tp_time,"          // 10 - track time
            "qoverp,"           // 11 - q over p
            "surface,"          // 12 - surface ID
            "loc0,"             // 13 - location on surface 0
            "loc1,"             // 14 - location on surface 1
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
            "trk_id,"           // 1 - track index
            "meas_time,"        // 2 - measurement time
            "meas_surface,"     // 3 - measurement surface ID
            "meas_loc0,"        // 4 - measurement local position 0
            "meas_loc1,"        // 5 - measurement local position 1
            "meas_cov0,"        // 6 - measurement covariance 0
            "meas_cov1,"        // 7 - measurement covariance 1
            "meas_cov_time,"    // 8 - measurement covariance time
            "hit_id,"           // 9 - tracker hit ID
            "hit_cell_id,"      // 10 - tracker hit cell ID
            "hit_x,"            // 11 - tracker hit x position
            "hit_y,"            // 12 - tracker hit y position
            "hit_z,"            // 13 - tracker hit z position
            "hit_time,"         // 14 - tracker hit time
            "hit_adc,"          // 15 - tracker hit ADC
            "mc_hit_id,"        // 16 - MC hit ID
            "mc_hit_plane,"     // 17 - MC hit plane
            "mc_hit_ring,"      // 18 - MC hit ring
            "mc_hit_pad,"       // 19 - MC hit pad
            "mc_hit_time,"      // 20 - MC hit time
            "mc_hit_adc,"       // 21 - MC hit ADC
            "mc_hit_ztogem,"    // 22 - MC hit z to gem
            "mc_hit_true_x,"    // 23 - MC hit true x
            "mc_hit_true_y,"    // 24 - MC hit true y
            "mc_hit_true_z"     // 25 - MC hit true z
            "\n");
    }

public:

    CsvWriterProcessor() {


        // Initialize logger
        m_log = spdlog::get("csv_writer");
        if (!m_log) {
            m_log = spdlog::default_logger()->clone("csv_writer");
        }

        SetTypeName(NAME_OF_THIS);  // Provide JANA with this class's name
        SetCallbackStyle(CallbackStyle::ExpertMode);
    }

    void Init() override {

        // Construct file names
        m_trackFileName = fmt::format("{}.in_tracks.csv", *m_cfg_filePrefix);
        m_hitFileName = fmt::format("{}.in_hits.csv", *m_cfg_filePrefix);

        // Open files
        m_trackFile.open(m_trackFileName);
        m_hitFile.open(m_hitFileName);

        if (!m_trackFile.is_open()) {
            throw JException("Failed to open track output file: %s", m_trackFileName.c_str());
        }
        if (!m_hitFile.is_open()) {
            throw JException("Failed to open hit output file: %s", m_hitFileName.c_str());
        }

        writeTrackHeader();
        writeHitHeader();

        m_log->info("Track output file: {}", m_trackFileName);
        m_log->info("Hit output file: {}", m_hitFileName);
    }


    void ProcessSequential(const JEvent& event) override {

        uint64_t eventNumber = event.GetEventNumber();

        // Process track seeds

            auto& seeds = *m_in_trackSeeds();
            m_log->info("seeds.size(): {}", seeds.size());

            for (auto trueSeed: seeds) {
                auto mcTrack = trueSeed.getMcTrack();

                // We MUST have this mcTrack
                if (!mcTrack.isAvailable()) {
                    auto message = fmt::format("!trueSeed.getMcTrack().isAvailable() SOME ERROR in data composition");
                    throw JException(message);
                }

                writeSeed(eventNumber, trueSeed);
                auto measurements = trueSeed.getMeasurements();
                for (auto trueMeasurement: measurements) {
                    writeMeasurement(eventNumber, trueSeed, trueMeasurement);
                }
            }

        // Process tracker hits
        try {


        } catch (const std::exception& e) {
            m_log->warn("Failed to process tracker hits for event {}: {}", eventNumber, e.what());
        }
    }

    void writeSeed(uint64_t eventIndex, const TrackSeed& seed) {

        auto mcTrack = seed.getMcTrack();
        auto params = seed.getInitParams();
        auto trkCov = params.getCovariance();
        auto loc = params.getLoc();
        auto perigee = seed.getPerigee();


        fmt::print(m_trackFile, "{},{}",
            eventIndex,                 // 0 - evt - event number/index
            seed.getObjectID().index,   // 1 - trk_id -  track index,
            mcTrack.getMomentum(),      // 2 - mc_mom - total momentum
            mcTrack.getPhi(),           // 3 - mc_phi - phi angle at start
            mcTrack.getTheta(),         // 4 - mc_theta - theta angle at start
            mcTrack.getVertexZ(),       // 5 - mc_vtx_z - Exact Z vertex
            mcTrack.getHits().size(),   // 6 - mc_hits_count - MC hits count
            params.getPdg(),            // 7 - pdg - init truth particle Pdg
            params.getPhi(),            // 8 - tp_phi - init truth parameters phi
            params.getTheta(),          // 9 - tp_theta - init truth parameters theta
            params.getTime(),           // 10 - getTrackTime
            params.getQOverP(),         // 11 - qOverP - init truth
            params.getSurface(),        // 12 - surface - init truth surface ID
            loc[0],                     // 13 - location on surface 0
            loc[1],                     // 14 - location on surface 1
            trkCov(0,0),        // 15 - cov_loc0 - init truth params covariance
            trkCov(1,1),        // 16 - cov_loc1 - init truth params covariance
            trkCov(2,2),        // 17 - cov_phi - init truth params covariance
            trkCov(3,3),        // 18 - cov_theta - init truth params covariance
            trkCov(4,4),        // 19 - cov_qoverp - init truth params covariance
            trkCov(5,5),        // 20 - cov_time - init truth params covariance
            perigee.x,              // 21 - perigee_x -
            perigee.y,              // 22 - perigee_y -
            perigee.z               // 23 - perigee_z -
        );

        // Hits are always sorted by time
        if (!mcTrack.getHits().empty()) {
            auto firstHit = mcTrack.getHits()[0];
            fmt::print(m_trackFile, ",{},{},{},{},{},{},{},{},{}",
                firstHit.getObjectID().index,       // 24 - fhit_id - the first hit index
                firstHit.getTime(),                 // 25 - fhit_time - the first hit time
                firstHit.getPlane(),                // 26 - fhit_plane - the first hit plane
                firstHit.getRing(),                 // 27 - fhit_ring - the first hit ring
                firstHit.getPad(),                  // 28 - fhit_pad - the frist hit pad
                firstHit.getZToGem(),               // 29 - fhit_ztogem - first hit z to gem
                firstHit.getTruePosition().x,       // 30 - fhit_true_x - the true x
                firstHit.getTruePosition().y,       // 31 - fhit_true_y - the true y
                firstHit.getTruePosition().z        // 32 - fhit_true_z - the true z
            );
        } else {
            fmt::print(m_trackFile, ",,,,,,,,,");
        }

        // end of record
        fmt::print(m_trackFile, "\n");
    }

    void writeMeasurement(uint64_t eventIndex, const TrackSeed& seed, const Measurement2D& measurement) {
        auto cov = measurement.getCovariance();
        auto loc = measurement.getLoc();
        auto trackerHit = measurement.getHits().at(0);  // Currently we know it should be there
        auto mcHit = trackerHit.getRawHit();            // It must be there
        auto hitPos = trackerHit.getPosition();
        
        fmt::print(m_hitFile, "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
            eventIndex,                     // 0 - evt - event number/index
            seed.getObjectID().index,       // 1 - trk_id - track index
            measurement.getTime(),          // 2 - meas_time - measurement time
            measurement.getSurface(),       // 3 - meas_surface - measurement surface ID
            loc[0],                         // 4 - meas_loc0 - measurement local position 0
            loc[1],                         // 5 - meas_loc1 - measurement local position 1
            cov[0],                         // 6 - meas_cov0 - measurement covariance 0
            cov[1],                         // 7 - meas_cov1 - measurement covariance 1
            cov[2],                         // 8 - meas_cov_time - measurement covariance time
            trackerHit.getObjectID().index, // 9 - hit_id - tracker hit ID
            trackerHit.getCellID(),         // 10 - hit_cell_id - tracker hit cell ID
            hitPos.x,                       // 11 - hit_x - tracker hit x position
            hitPos.y,                       // 12 - hit_y - tracker hit y position
            hitPos.z,                       // 13 - hit_z - tracker hit z position
            trackerHit.getTime(),           // 14 - hit_time - tracker hit time
            trackerHit.getEDep(),           // 15 - hit_adc - tracker hit ADC (energy deposit)
            mcHit.getObjectID().index,      // 16 - mc_hit_id - MC hit ID
            mcHit.getPlane(),               // 17 - mc_hit_plane - MC hit plane
            mcHit.getRing(),                // 18 - mc_hit_ring - MC hit ring
            mcHit.getPad(),                 // 19 - mc_hit_pad - MC hit pad
            mcHit.getTime(),                // 20 - mc_hit_time - MC hit time
            mcHit.getAdc(),                 // 21 - mc_hit_adc - MC hit ADC
            mcHit.getZToGem(),              // 22 - mc_hit_ztogem - MC hit z to gem
            mcHit.getTruePosition().x,      // 23 - mc_hit_true_x - MC hit true x
            mcHit.getTruePosition().y,      // 24 - mc_hit_true_y - MC hit true y
            mcHit.getTruePosition().z       // 25 - mc_hit_true_z - MC hit true z
        );

        // end of record
        fmt::print(m_hitFile, "\n");
    }

    void Finish() override {
        if (m_trackFile.is_open()) {
            m_trackFile.close();
            m_log->info("Closed track output file: {}", m_trackFileName);
        }
        if (m_hitFile.is_open()) {
            m_hitFile.close();
            m_log->info("Closed hit output file: {}", m_hitFileName);
        }
    }
};

}   // namespace tdis::io