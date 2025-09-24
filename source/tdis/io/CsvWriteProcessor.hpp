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

namespace tdis::io{
class CsvWriterProcessor : public JEventProcessor {


    // Parameters following m_cfg_xxxYyy convention
    Parameter<std::string> m_cfg_filePrefix {this,
        "csv:file", "output",
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
        try {




        } catch (const std::exception& e) {
            m_log->warn("Failed to process track seeds for event {}: {}", eventNumber, e.what());
        }

        // Process tracker hits
        try {


        } catch (const std::exception& e) {
            m_log->warn("Failed to process tracker hits for event {}: {}", eventNumber, e.what());
        }
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