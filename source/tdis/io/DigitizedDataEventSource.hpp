// Created by Dmitry Romanov, somewhere in 2024
// Subject to the terms in the LICENSE file found in the top-level directory.

/**
 *  This file contains EventSource for text files containing TDIS digitized tracks in text format
 *  Each track starts with "Event <event number>* don't be confused, one event - one track here
 *  Then goes a line with track info, 4 variables as
 *      1. Momentum (GeV/c)
 *      2. Theta (degrees)
 *      3. Phi (degrees)
 *      4. Z vertex (m)
 *  Then a line for each hit
 *    - Time of arrival at Pad (ns)
 *    - Amplitude (ADC bin of sampa)
 *    - Ring (id of rin, 0 is innermost).
 *    - Pad (id of pad, 0 is at or closest to phi=0 and numbering is clockwise).
 *    - Plane(id of z plane from 0 upstream  to 9 downstream)
 *    - ZtoGEM (m)
 *    - TrueX (m?)?
 *    - TrueY (m?)?
 *    - TrueZ (m?)?
 *  (?) Some files do not have TrueXYZ information
 *
 *   Example of how it looks:
  Event 200000
	0.922362	89.49	-156.71	-0.0605
	312.855	2.05888e-08	0	68	3	0.0100347
	312.577	7.19261e-08	0	68	3	0.0100256
	285.625	2.7713e-08	20	68	3	0.0091457
	...
  Event 200001
        ...

 **/
#pragma once

#include <JANA/JApplication.h>
#include <JANA/JEvent.h>
#include <JANA/JEventSource.h>
#include <JANA/JEventSourceGeneratorT.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <Acts/Definitions/Units.hpp>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "podio_model/DigitizedMtpcMcHitCollection.h"
#include "podio_model/DigitizedMtpcMcTrackCollection.h"
#include "podio_model/EventInfoCollection.h"
#include "services/LogService.hpp"

namespace tdis::io {

    /** POD structure for readout hits **/
    struct DigitizedReadoutHit {
        double time;     // - Time of arrival at Pad (ns)
        double adc;      // - Amplitude (ADC bin of sample)
        int ring;        // - Ring (id of rin, 0 is innermost).
        int pad;         // - Pad (id of pad, 0 is at or closest to phi=0 and numbering is clockwise).
        int plane;       // - Plane(id of z plane from 0 upstream  to 9 downstream)
        double zToGem;   // - ZtoGEM (m)
        double true_x;   // - True hit x info (quiet_NaN() if not provided)
        double true_y;   // - True hit y info (quiet_NaN() if not provided)
        double true_z;   // - True hit z info (quiet_NaN() if not provided)
    };

    /** POD structure for readout track **/
    struct DigitizedReadoutTrack {
        double momentum;    // (GeV/c)
        double theta;       // (degrees)
        double phi;         // (degrees)
        double vertexZ;     // (m)
        std::vector<DigitizedReadoutHit> hits;
    };

    /** Digitized files in text format EventSource */
    class DigitizedDataEventSource : public JEventSource {

        std::ifstream m_input_file;
        size_t m_current_line_index = 0;
        std::shared_ptr<spdlog::logger> m_log;

    public:
        DigitizedDataEventSource();

        DigitizedDataEventSource(std::string resource_name, JApplication* app);

        ~DigitizedDataEventSource() override = default;

        /// Initializes class (after all JANA2 services are ready to serve)
        void Init() override;

        /// Opens file to read
        void Open() override;

        /// Closes the file
        void Close() override;

        /// Read file and forms an event based on it
        Result Emit(JEvent&) override;

        /// Do we need it at all?
        static std::string GetDescription();

        Parameter<int> m_tracks_per_event{this, "io:tracks_per_event", 1, "Number of tracks to combine per event"};
        Service<services::LogService> m_log_svc{this};
    private:

        /// Parses string tokens to form DigitizedReadoutHit
        bool ParseTrackHit(const std::vector<std::string>& tokens, DigitizedReadoutHit& result);

        /// Parses string tokens to form DigitizedReadoutTrack
        bool ParseTrackHeader(const std::vector<std::string>& tokens, DigitizedReadoutTrack& result);
    };


    // Implementation section starts here

    inline DigitizedDataEventSource::DigitizedDataEventSource() : JEventSource() {
        SetTypeName(NAME_OF_THIS);  // Provide JANA with class name
        SetCallbackStyle(CallbackStyle::ExpertMode);
    }

    inline DigitizedDataEventSource::DigitizedDataEventSource(std::string resource_name, JApplication* app): JEventSource(resource_name, app) {
        SetTypeName(NAME_OF_THIS);  // Provide JANA with class name
        SetCallbackStyle(CallbackStyle::ExpertMode);
    }

    inline void DigitizedDataEventSource::Init() {
        auto app = GetApplication();
        m_log = m_log_svc->logger("io");
        m_log->info("Our log level is: ", services::LogLevelToString(m_log->level()));
        m_log->info("Number tracks per event is: {}", m_tracks_per_event());
    }

    inline void DigitizedDataEventSource::Open() {
        // Open the file
        m_input_file = std::ifstream(this->GetResourceName());

        // Check if the file was successfully opened
        if (!m_input_file.is_open()) {
            auto message= fmt::format("Error: Could not open the file: '{}'", this->GetResourceName());
            m_log->error(message);
            throw std::runtime_error(message);
        }
    }

    inline void DigitizedDataEventSource::Close() {
        // Close the file pointer here!
        m_input_file.close();
    }

    inline void PrintStreamError(const std::ifstream& file) {
        if (file.bad()) {
            std::cerr << "Stream has a badbit set. This could indicate a serious I/O error, such as hardware failure." << std::endl;
        } else if (file.fail()) {
            if (file.eof()) {
                std::cerr << "Reached end of file." << std::endl;
            } else {
                std::cerr << "Logical error on i/o operation (failbit set)." << std::endl;
            }
        } else if (file.eof()) {
            std::cout << "End of file reached." << std::endl;
        }
    }


    static std::vector<std::string> SplitDataString(const std::string& line) {
        std::vector<std::string> tokens;
        const char* str = line.data();
        const char* end = str + line.size();

        while (str < end) {
            // Skip leading whitespace
            while (str < end && std::isspace(static_cast<unsigned char>(*str))) {
                ++str;
            }

            if (str >= end) break;

            // Start of the token
            const char* token_start = str;

            // Find the end of the token
            while (str < end && !std::isspace(static_cast<unsigned char>(*str))) {
                ++str;
            }

            // Extract the token and add it to the vector
            tokens.emplace_back(token_start, str - token_start);
        }

        return tokens;
    }


    static std::vector<std::string> ReadNextEventLines(std::ifstream& input_file) {
        std::vector<std::string> lines;

        while (true) {
            if (!input_file) {
                return lines;
            }

            // Read the file line by line
            std::string line;
            std::getline(input_file, line);

            if (line.starts_with("Event")) {
                if (lines.empty()) {
                    // This probably means the beginning of file, the first event
                    continue;
                }
                // This means we read till the next events
                return lines;
            }

            lines.emplace_back(line);
        }
    }

    inline bool DigitizedDataEventSource::ParseTrackHeader(const std::vector<std::string>& tokens, DigitizedReadoutTrack& result) {
        if (tokens.size() < 4) {
            m_log->warn("Could not parse track info. Incorrect tokens number {}. Near line: {}", tokens.size(), m_current_line_index);
            return false;
        }

        result.momentum = std::stod(tokens[0]);  // (GeV/c)
        result.theta = std::stod(tokens[1]);     // (degrees)
        result.phi = std::stod(tokens[2]);       // (degrees)
        result.vertexZ = std::stod(tokens[3]);   // (m)
        return true;
    }

    inline bool DigitizedDataEventSource::ParseTrackHit(const std::vector<std::string>& tokens, DigitizedReadoutHit& result) {
        if (tokens.empty()) {
            m_log->info("Could not parse track hit. Tokens are empty. Empty line? Near line: {}", m_current_line_index);
            return false;
        }
        if (tokens.size() < 6) {
            m_log->warn("Could not parse track hit. Incorrect tokens number {}. Near line: {}", tokens.size(), m_current_line_index);
            return false;
        }

        if(tokens.size() == 6) {
            // Files with no true X Y Z hit info
            result.time   = std::stod(tokens[0]);    // - Time of arrival at Pad (ns)
            result.adc    = std::stod(tokens[1]);    // - Amplitude (ADC bin of sample)
            result.ring   = std::stoi(tokens[2]);    // - Ring (id of rin, 0 is innermost).
            result.pad    = std::stoi(tokens[3]);    // - Pad (id of pad, 0 is at or closest to phi=0 and numbering is clockwise).
            result.plane  = std::stoi(tokens[4]);    // - Plane(id of z plane from 0 upstream  to 9 downstream)
            result.zToGem = std::stod(tokens[5]);    // - ZtoGEM (m)

            // True hit information is not set
            result.true_x = std::numeric_limits<double>::quiet_NaN();
            result.true_y = std::numeric_limits<double>::quiet_NaN();
            result.true_z = std::numeric_limits<double>::quiet_NaN();
        } else {
            result.time   = std::stod(tokens[0]);    // - Time of arrival at Pad (ns)
            result.adc    = std::stod(tokens[1]);    // - Amplitude (ADC bin of sample)
            result.true_x = std::stod(tokens[2]);    // True X Y Z of hit
            result.true_y = std::stod(tokens[3]);
            result.true_z = std::stod(tokens[4]);
            result.ring   = std::stoi(tokens[5]);    // - Ring (id of rin, 0 is innermost).
            result.pad    = std::stoi(tokens[6]);    // - Pad (id of pad, 0 is at or closest to phi=0 and numbering is clockwise).
            result.plane  = std::stoi(tokens[7]);    // - Plane(id of z plane from 0 upstream  to 9 downstream)
            result.zToGem = std::stod(tokens[8]);    // - ZtoGEM (m)
        }

        return true;
    }

    inline JEventSource::Result DigitizedDataEventSource::Emit(JEvent& event) {
        // Calls to GetEvent are synchronized with each other, which means they can
        // read and write state on the JEventSource without causing race conditions.

        static size_t current_event_number = 1;
        event.SetEventNumber(current_event_number++);
        event.SetRunNumber(22);

        auto lines = ReadNextEventLines(m_input_file);
        m_log->debug("Number of lines per event: {}", lines.size());

        if (lines.empty()) {
            if (m_input_file.bad() || m_input_file.fail() || m_input_file.eof()) {
                PrintStreamError(m_input_file);
            } else {
                m_log->error("Event is empty. Near line: {}", m_current_line_index);
            }

            return Result::FailureFinished;
        }

        size_t m_event_line_index = m_current_line_index;

        // (!) Each new event starts with Event word, which is thrown out by ReadNextEventLines
        // but we need to count it in m_current_line_index
        m_current_line_index++;

        // First line is alsways track/event header
        if (lines.size() == 1) {
            m_log->debug("Empty event at line (near): {}\n", m_current_line_index);
            m_current_line_index++;
            return Result::FailureTryAgain;
        }

        // First we parse event into DigitizedReadoutTrack with DigitizedReadoutHits
        // Then we will copy it to PODIO structures
        // We do this extra step because in future we want to merge tracks in various ways

        // Parse track
        DigitizedReadoutTrack track{};
        try {
            auto tokens = SplitDataString(lines[0]);
            if(!ParseTrackHeader(tokens, track)) {
                return Result::FailureFinished;
            }

            for (auto i = 1; i < lines.size(); ++i) {
                DigitizedReadoutHit hit{};
                tokens = SplitDataString(lines[i]);
                if(!ParseTrackHit(tokens, hit)) {
                    continue;
                }
                track.hits.emplace_back(hit);
            }

            m_current_line_index += lines.size();
        } catch (...) {
            fmt::print("Error parsing event/track. Near line: {}\n", m_current_line_index);
            throw;
        }

        // Double check that we have some track with some hits
        if(track.hits.empty()) {
            m_log->warn("Could not parse track hit. WE SHOULDN'T BE HERE. Near line: {}", m_current_line_index);
            return Result::FailureFinished;
        }

        // Copy data to PODIO
        DigitizedMtpcMcTrackCollection podioTracks;
        DigitizedMtpcMcHitCollection podioHits;
        auto podioTrack = podioTracks.create();
        podioTrack.setPhi(track.phi);
        podioTrack.setTheta(track.theta);
        podioTrack.setVertexZ(track.vertexZ);
        podioTrack.setMomentum(track.momentum);
        for(auto& hit: track.hits) {

            auto podioHit = podioHits.create();
            podioHit.setTime(   hit.time * Acts::UnitConstants::ns  );
            podioHit.setAdc(    hit.adc   );
            podioHit.setRing(   hit.ring  );
            podioHit.setPad(    hit.pad   );
            podioHit.setPlane(  hit.plane );
            podioHit.setZToGem( hit.zToGem  * Acts::UnitConstants::m);

            tdis::Vector3f true_pos = tdis::Vector3f{
                static_cast<float>(hit.true_x * Acts::UnitConstants::m),
                static_cast<float>(hit.true_y * Acts::UnitConstants::m),
                static_cast<float>(hit.true_z * Acts::UnitConstants::m)
            };
            podioHit.setTruePosition(true_pos);
            podioTrack.addToHits(podioHit);
        }

        EventInfoCollection info;
        info.push_back(MutableEventInfo(0, 0, 0)); // event nr, timeslice nr, run nr
        event.InsertCollection<EventInfo>(std::move(info), "EventInfo");
        event.InsertCollection<DigitizedMtpcMcTrack>(std::move(podioTracks), "DigitizedMtpcMcTrack");
        event.InsertCollection<DigitizedMtpcMcHit>(std::move(podioHits), "DigitizedMtpcMcHit");
        m_log->info("Event has been emitted at {}", m_event_line_index);
        return Result::Success;
    }

    inline std::string DigitizedDataEventSource::GetDescription() {
        // GetDescription() helps JANA explain to the user what is going on
        return "Digitized TDIS MTPC .txt file event source";
    }
} // namespace tdis



// The template specialization needs to be in the global namespace (or at least not inside the tdis namespace)
template <>
inline double JEventSourceGeneratorT<tdis::io::DigitizedDataEventSource>::CheckOpenable(std::string resource_name) {
  // CheckOpenable() decides how confident we are that this EventSource can handle this resource.
  //    0.0        -> 'Cannot handle'
  //    (0.0, 1.0] -> 'Cean handle, with this confidence level'

  // To determine confidence level, feel free to open up the file and check for magic bytes or metadata.
  // Returning a confidence <- {0.0, 1.0} is perfectly OK!
    bool is_correct_ext = resource_name.ends_with("txt");
    return  is_correct_ext? 1.0 : 0.0;
}