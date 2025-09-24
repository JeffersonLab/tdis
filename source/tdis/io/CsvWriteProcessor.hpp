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
                "evt,"
                "trk_id,"
                "\n");


    }

    void writeHitHeader() {

            fmt::print(m_hitFile,
                "evt,"
                "trk_id,"
                "hit_id,"
                "hit_plane,"
                "hit_ring,"
                "hit_pad"
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
        auto trackerHit = measurement.getHits().at(0);  // Currently we know it should be tehre
        auto mcHit = trackerHit.getRawHit();              // It must be there
        fmt::print(m_hitFile, "{},{}",
            eventIndex,                 // 0 - evt - event number/index
            seed.getObjectID().index,   // 1 - trk_id -  track index,
            measurement.getTime(),      // - time -

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