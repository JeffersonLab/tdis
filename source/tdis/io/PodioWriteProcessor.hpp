// Created by Dmitry Romanov, somewhere in 2024
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

/** This class is responsible for writing out PODIO Collections to the output podio file
 *
 * (!) In the current version of PODIO, the customization of what is written from what is added to frame is limited
 * It is suggested to write everything that is created, to avoid segfaults and side effects
 *
 * (!!!) If you create PODIO data, add collections names below to output_collections
 * This means that if you create and fill some PODIO collection, you ensure it is written by this processor.
 *
 * Configuration Parameters
 *    - podio:output_file (std::string):
 *        The name of the output ROOT file. Default is "podio_output.root".
 *        Setting this parameter to "1" uses the default file name.
 *
 *    - podio:print_collections (std::vector<std::string>):
 *        A comma-separated list of collection names to print to the console for debugging purposes.
 *
 *    - podio:output_collections (std::vector<std::string>): '
 *            A comma-separated list of collection names to write out.
 *            If not set, all collections will be written.
 *            It's not recommended to specify this flag but one can use it for some debug purposes
 *
 * Important Considerations:
 *
 *  - Consistency Requirement: All collections intended to be written must be present in the first event.
 *    This is due to a constraint in podio::ROOTWriter, which requires all branches (collections)
 *    to be defined when the first event is written.

 *  - Exception Handling: Missing Collections: If a collection is missing or fails to be retrieved,
 *    the processor catches the exception and marks the collection as failed.
 *    Skipping Failed Collections: Failed collections are skipped in subsequent events to prevent
 *    repeated exceptions and potential crashes.
 */

#include <JANA/JApplication.h>
#include <JANA/JEvent.h>
#include <JANA/JEventProcessor.h>
#include <JANA/JLogger.h>
#include <JANA/Services/JParameterManager.h>
#include <JANA/Utils/JTypeInfo.h>
#include <fmt/core.h>
#include <podio/CollectionBase.h>
#include <podio/Frame.h>
#include <podio/ROOTWriter.h>
#include <podio/podioVersion.h>
#include <spdlog/logger.h>

#include <ActsExamples/EventData/Track.hpp>
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "podio_model/Track.h"
#include "podio_model/TrackerHit.h"
#include "services/LogService.hpp"

namespace tdis::io {
class PodioWriteProcessor : public JEventProcessor {

public:
    // Get the list of output collections to include/exclude
    std::vector<std::string> output_collections = {
        // Header and other metadata
        "EventInfo",

        // Truth record
        "DigitizedMtpcMcTrack",
        "DigitizedMtpcMcHit",


        "TrackerHit",
        "Measurement2D",
        "TruthTrackInitParameters",
        "FittedTrajectories", "FittedTrackParams", "FittedTracks"
    };

  PodioWriteProcessor(JApplication * app);
  ~PodioWriteProcessor() override = default;

  void Init() override;
  void Process(const std::shared_ptr<const JEvent>& event) override;
  void Finish() override;


  std::unique_ptr<podio::ROOTWriter> m_writer;

  std::mutex m_mutex;
  bool m_is_first_event = true;
  std::shared_ptr<spdlog::logger> m_log;
  std::string m_output_file = "podio_output.root";

  std::set<std::string> m_output_collections;  // config. parameter
  std::vector<std::string> m_collections_to_write;  // derived from above config. parameters
  std::vector<std::string> m_collections_to_print;
  JApplication * m_app;

};


inline PodioWriteProcessor::PodioWriteProcessor(JApplication* app) {
    m_app = app;
    SetTypeName(NAME_OF_THIS);  // Provide JANA with this class's name
}

inline void PodioWriteProcessor::Init() {
    auto* app = m_app;
    m_log = app->GetService<tdis::services::LogService>()->logger("PodioWriteProcessor");

    m_app->SetDefaultParameter("podio:output_file", m_output_file, "Podio output file to write to");

    // Allow user to set PODIO:OUTPUT_FILE to "1" to specify using the default name.
    if (m_output_file == "1") {
        auto param = m_app->GetJParameterManager()->FindParameter("podio:output_file");
        if (param) {
            param->SetValue(param->GetDefault());
            m_output_file = param->GetDefault();
        }
    }

    m_app->SetDefaultParameter(
        "podio:output_collections", output_collections,
        "Comma separated list of collection names to write out. If not set, all collections will "
        "be written (including ones from input file). Don't set this and use "
        "PODIO:OUTPUT_EXCLUDE_COLLECTIONS to write everything except a selection.");

    m_output_collections
        = std::set<std::string>(output_collections.begin(), output_collections.end());

    m_app->SetDefaultParameter(
        "podio:print_collections", m_collections_to_print,
        "Comma separated list of collection names to print to screen, e.g. for debugging.");

    m_writer = std::make_unique<podio::ROOTWriter>(m_output_file);
}

inline void PodioWriteProcessor::Process(const std::shared_ptr<const JEvent>& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // auto hits = event->GetCollection<tdis::TrackerHit>("TrackerHit");

    [[maybe_unused]] auto tracks = event->GetCollection<tdis::Track>("FittedTracks");

    m_log->info("PodioWriteProcessor::Process() All event collections:");
    auto event_collections = event->GetAllCollectionNames();
    for (const auto& coll_name : event_collections) {
        try {
            m_log->info("   {}", coll_name);
        } catch (std::exception& e) {
            // chomp
        }
    }


    // Trigger all collections once to fix the collection IDs
    m_collections_to_write.clear();
    for (const auto& coll_name : m_output_collections) {
        try {
            [[maybe_unused]] const auto* coll_ptr = event->GetCollectionBase(coll_name);
            m_collections_to_write.push_back(coll_name);
        } catch (std::exception& e) {
            m_log->warn("Exception trying to produce: {}, message:  {}", coll_name, e.what());
            // chomp
        }
    }

    // Print the contents of some collections, just for debugging purposes
    // Do this before writing just in case writing crashes
    if (!m_collections_to_print.empty()) {
        LOG << "========================================" << LOG_END;
        LOG << "PodioWriteProcessor: Event " << event->GetEventNumber() << LOG_END;
    }
    for (const auto& coll_name : m_collections_to_print) {
        LOG << "------------------------------" << LOG_END;
        LOG << coll_name << LOG_END;
        try {
            const auto* coll_ptr = event->GetCollectionBase(coll_name);
            if (coll_ptr == nullptr) {
                LOG << "missing" << LOG_END;
            } else {
                coll_ptr->print();
            }
        } catch (std::exception& e) {
            LOG << "missing" << LOG_END;
        }
    }

    m_log->trace("==================================");
    m_log->trace("Event #{}", event->GetEventNumber());

    // Make sure that all factories get called that need to be written into the frame.
    // We need to do this for _all_ factories unless we've constrained it by using
    // includes/excludes. Note that all collections need to be present in the first event, as
    // podio::RootFrameWriter constrains us to write one event at a time, so there is no way to add
    // a new branch after the first event.

    // If we get an exception below while trying to add a factory for any
    // reason then mark that factory as bad and don't try running it again.
    // This is motivated by trying to write EcalBarrelSciGlass objects for
    // data simulated using the imaging calorimeter. In that case, it will
    // always throw an exception, but DD4hep also prints its own error message.
    // Thus, to prevent that error message every event, we must avoid calling
    // it.

    // Activate factories.
    // TODO: NWB: For now we run every factory every time, swallowing exceptions if necessary.
    //            We do this so that we always have the same collections created in the same order.
    //            This means that the collection IDs are stable so the writer doesn't segfault.
    //            The better fix is to maintain a map of collection IDs, or just wait for PODIO to
    //            fix the bug.
    std::vector<std::string> successful_collections;
    static std::set<std::string> failed_collections;
    for (const std::string& coll : m_collections_to_write) {
        try {
            m_log->trace("Ensuring factory for collection '{}' has been called.", coll);
            const auto* coll_ptr = event->GetCollectionBase(coll);
            if (coll_ptr == nullptr) {
                // If a collection is missing from the frame, the podio root writer will segfault.
                // To avoid this, we treat this as a failing collection and omit from this point
                // onwards. However, this code path is expected to be unreachable because any
                // missing collection will be replaced with an empty collection in
                // JFactoryPodioTFixed::Create.
                if (failed_collections.count(coll) == 0) {
                    m_log->error("Omitting PODIO collection '{}' because it is null", coll);
                    failed_collections.insert(coll);
                }
            } else {
                m_log->trace("Including PODIO collection '{}'", coll);
                successful_collections.push_back(coll);
            }
        } catch (std::exception& e) {
            // Limit printing warning to just once per factory
            if (failed_collections.count(coll) == 0) {
                m_log->error("Omitting PODIO collection '{}' due to exception: {}.", coll,
                             e.what());
                failed_collections.insert(coll);
            }
        }
    }
    m_collections_to_write = successful_collections;

    // Frame will contain data from all Podio factories that have been triggered,
    // including by the `event->GetCollectionBase(coll);` above.
    // Note that collections MUST be present in frame. If a collection is null, the writer will
    // segfault.
    const auto* frame = event->GetSingle<podio::Frame>();

    // TODO: NWB: We need to actively stabilize podio collections. Until then, keep this around in
    // case
    //            the writer starts segfaulting, so we can quickly see whether the problem is
    //            unstable collection IDs.
    /*
    m_log->info("Event {}: Writing {} collections", event->GetEventNumber(),
    m_collections_to_write.size()); for (const std::string& collname : m_collections_to_write) {
        m_log->info("Writing collection '{}' with id {}", collname, frame->get(collname)->getID());
    }
    */
    m_writer->writeFrame(*frame, "events", m_collections_to_write);

    auto [missing_names, all_names] = m_writer->checkConsistency(m_collections_to_write, "");
    m_log->info("PODIO checkConsistency missing_names: {}", missing_names.size());
    for (const auto& coll_name : missing_names) {
        m_log->info("   {}", coll_name);
    }
    m_is_first_event = false;
}

inline void PodioWriteProcessor::Finish() {
    m_writer->finish();
}

}   // namespace tdis:io