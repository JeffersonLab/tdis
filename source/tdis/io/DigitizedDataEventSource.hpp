// Created by Dmitry Romanov, somewhere in 2024
// Subject to the terms in the LICENSE file found in the top-level directory.

/**
 *  EventSource for text files containing TDIS digitised tracks.
 *
 *  File format
 *  -----------
 *  Each track begins with an "Event <N>" header line (one event = one track).
 *  The next line holds four track-level values:
 *      1. Momentum  (GeV/c)
 *      2. Theta     (degrees)
 *      3. Phi       (degrees)
 *      4. Z vertex  (m)
 *
 *  Each subsequent line is one hit:
 *      1. Time of arrival at pad  (ns)
 *      2. Amplitude               (ADC bin of SAMPA)
 *      3. Ring   (0 = innermost)
 *      4. Pad    (0 at φ=0, clockwise)
 *      5. Plane  (0 upstream … 9 downstream)
 *      6. ZtoGEM (m)
 *      7. TrueX  (m)   ← optional; absent in some files
 *      8. TrueY  (m)   ← optional
 *      9. TrueZ  (m)   ← optional
 *
 *  Example:
 *
 *  Event 200000
 *    0.922362   89.49  -156.71  -0.0605
 *    312.855  2.05888e-08  0  68  3  0.0100347
 *    312.577  7.19261e-08  0  68  3  0.0100256
 *    ...
 *  Event 200001
 *    ...
 */

#pragma once

#include <JANA/JEvent.h>
#include <JANA/JEventSource.h>
#include <JANA/JEventSourceGeneratorT.h>

#include <Acts/Definitions/Units.hpp>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "geometry/TdisGeometryHelper.hpp"
#include "logging/LogService.hpp"
#include "podio_model/DigitizedMtpcMcHitCollection.h"
#include "podio_model/DigitizedMtpcMcTrackCollection.h"
#include "podio_model/EventInfoCollection.h"

namespace tdis::io {

// ---------------------------------------------------------------------------
// POD helpers for in-memory representation before copying to PODIO objects
// ---------------------------------------------------------------------------

struct DigitizedReadoutHit {
    double time;    // Time of arrival at pad (ns)
    double adc;     // Amplitude (ADC bin of SAMPA)
    int    ring;    // Ring index (0 = innermost)
    int    pad;     // Pad index (0 at φ=0, clockwise)
    int    plane;   // Plane index (0 upstream … 9 downstream)
    double zToGem;  // Distance to GEM (m)
    double true_x;  // True hit X (m); quiet_NaN when not provided
    double true_y;  // True hit Y (m); quiet_NaN when not provided
    double true_z;  // True hit Z (m); quiet_NaN when not provided
};

struct DigitizedReadoutTrack {
    double momentum;  // (GeV/c)
    double theta;     // (degrees)
    double phi;       // (degrees)
    double vertexZ;   // (m)
    std::vector<DigitizedReadoutHit> hits;
};

// ---------------------------------------------------------------------------
// Event source
// ---------------------------------------------------------------------------

class DigitizedDataEventSource : public JEventSource {

    std::ifstream m_input_file;
    size_t        m_current_line_index = 0;
    std::shared_ptr<spdlog::logger> m_log;

public:
    DigitizedDataEventSource();
    DigitizedDataEventSource(std::string fileName, JApplication* app);
    ~DigitizedDataEventSource() override = default;

    /// Called after all JANA2 services are ready.
    void Init() override;

    /// Opens the input file.
    void Open() override;

    /// Closes the input file.
    void Close() override;

    /// Reads one event from the file.
    Result Emit(JEvent&) override;

    static std::string GetDescription();

    Parameter<int> m_tracks_per_event{
        this,
        "DigitizedDataEventSource:tracks_per_event",
        1,
        "Number of tracks to combine per event"};

    Service<services::LogService> m_log_svc{this};

private:
    /// Parses a hit line token vector into a DigitizedReadoutHit.
    bool ParseTrackHit(const std::vector<std::string>& tokens,
                       DigitizedReadoutHit&             result);

    /// Parses a track-header token vector into a DigitizedReadoutTrack.
    bool ParseTrackHeader(const std::vector<std::string>& tokens,
                          DigitizedReadoutTrack&           result);
};

// ===========================================================================
// Implementation
// ===========================================================================

inline DigitizedDataEventSource::DigitizedDataEventSource() : JEventSource() {
    SetTypeName(NAME_OF_THIS);
    SetCallbackStyle(CallbackStyle::ExpertMode);
}

inline DigitizedDataEventSource::DigitizedDataEventSource(
    std::string fileName, JApplication* app)
    : JEventSource(fileName, app)
{
    SetTypeName(NAME_OF_THIS);
    SetCallbackStyle(CallbackStyle::ExpertMode);
}

inline void DigitizedDataEventSource::Init() {
    m_log = m_log_svc->logger("DigitizedDataEventSource");
    // FIX (minor): original fmt::print call had a format placeholder but
    // passed the log level string as the second argument to m_log->info
    // using the wrong overload.  Use a proper format string instead.
    m_log->info("Log level: {}", services::LogLevelToString(m_log->level()));
    m_log->info("Tracks per event: {}", m_tracks_per_event());
}

inline void DigitizedDataEventSource::Open() {
    m_input_file = std::ifstream(this->GetResourceName());
    if (!m_input_file.is_open()) {
        auto msg = fmt::format("Could not open file: '{}'", this->GetResourceName());
        m_log->error(msg);
        throw std::runtime_error(msg);
    }
}

inline void DigitizedDataEventSource::Close() {
    m_input_file.close();
}

// ---------------------------------------------------------------------------
// Internal helpers (file-local linkage)
// ---------------------------------------------------------------------------

inline void PrintStreamError(const std::ifstream& file) {
    if (file.bad()) {
        std::cerr << "Stream badbit set — possible hardware I/O error.\n";
    } else if (file.fail()) {
        if (file.eof())
            std::cerr << "Reached end of file.\n";
        else
            std::cerr << "Logical I/O error (failbit set).\n";
    } else if (file.eof()) {
        std::cout << "End of file reached.\n";
    }
}

static std::vector<std::string> SplitDataString(const std::string& line) {
    std::vector<std::string> tokens;
    const char* str = line.data();
    const char* end = str + line.size();

    while (str < end) {
        // Skip leading whitespace
        while (str < end && std::isspace(static_cast<unsigned char>(*str)))
            ++str;

        if (str >= end) break;

        const char* token_start = str;
        while (str < end && !std::isspace(static_cast<unsigned char>(*str)))
            ++str;

        tokens.emplace_back(token_start, str - token_start);
    }
    return tokens;
}

/// Reads lines for the next event.
/// The "Event <N>" header line is consumed but not included in the returned
/// vector.  The first returned line is the track-parameter line.
static std::vector<std::string> ReadNextEventLines(std::ifstream& input_file) {
    std::vector<std::string> lines;

    while (true) {
        if (!input_file)
            return lines;

        std::string line;
        std::getline(input_file, line);

        if (line.starts_with("Event")) {
            if (lines.empty()) {
                // Beginning of file — this is the very first Event header.
                continue;
            }
            // We have already collected lines for the current event and have
            // now read the header of the *next* event.  Return what we have.
            return lines;
        }

        lines.emplace_back(line);
    }
}

// ---------------------------------------------------------------------------
// ParseTrackHeader
// ---------------------------------------------------------------------------
inline bool DigitizedDataEventSource::ParseTrackHeader(
    const std::vector<std::string>& tokens,
    DigitizedReadoutTrack&           result)
{
    if (tokens.size() < 4) {
        m_log->warn("Cannot parse track header: expected ≥4 tokens, got {} near line {}",
                    tokens.size(), m_current_line_index);
        return false;
    }
    result.momentum = std::stod(tokens[0]);   // GeV/c
    result.theta    = std::stod(tokens[1]);   // degrees
    result.phi      = std::stod(tokens[2]);   // degrees
    result.vertexZ  = std::stod(tokens[3]);   // m
    return true;
}

// ---------------------------------------------------------------------------
// ParseTrackHit
// ---------------------------------------------------------------------------
inline bool DigitizedDataEventSource::ParseTrackHit(
    const std::vector<std::string>& tokens,
    DigitizedReadoutHit&             result)
{
    if (tokens.empty()) {
        m_log->info("Empty hit line near line {}", m_current_line_index);
        return false;
    }
    if (tokens.size() < 6) {
        m_log->warn("Cannot parse hit: expected ≥6 tokens, got {} near line {}",
                    tokens.size(), m_current_line_index);
        return false;
    }

    if (tokens.size() == 6) {
        // Older files without true XYZ
        result.time   = std::stod(tokens[0]);   // ns
        result.adc    = std::stod(tokens[1]);   // ADC counts
        result.ring   = std::stoi(tokens[2]);
        result.pad    = std::stoi(tokens[3]);
        result.plane  = std::stoi(tokens[4]);
        result.zToGem = std::stod(tokens[5]);   // m

        result.true_x = std::numeric_limits<double>::quiet_NaN();
        result.true_y = std::numeric_limits<double>::quiet_NaN();
        result.true_z = std::numeric_limits<double>::quiet_NaN();
    } else {
        // Newer files with true XYZ (columns 2-4)
        result.time   = std::stod(tokens[0]);   // ns
        result.adc    = std::stod(tokens[1]);   // ADC counts
        result.true_x = std::stod(tokens[2]);   // m
        result.true_y = std::stod(tokens[3]);   // m
        result.true_z = std::stod(tokens[4]);   // m
        result.ring   = std::stoi(tokens[5]);
        result.pad    = std::stoi(tokens[6]);
        result.plane  = std::stoi(tokens[7]);
        result.zToGem = std::stod(tokens[8]);   // m
    }
    return true;
}

// ---------------------------------------------------------------------------
// Emit
// ---------------------------------------------------------------------------
inline JEventSource::Result DigitizedDataEventSource::Emit(JEvent& event) {
    // Emit() is called from a single thread, so static state is safe here.
    static size_t current_event_number = 0;
    event.SetEventNumber(current_event_number++);
    event.SetRunNumber(22);

    auto lines = ReadNextEventLines(m_input_file);
    m_log->debug("Lines read for this event: {}", lines.size());

    if (lines.empty()) {
        if (m_input_file.bad() || m_input_file.fail() || m_input_file.eof())
            PrintStreamError(m_input_file);
        else
            m_log->error("Empty event near line {}", m_current_line_index);
        return Result::FailureFinished;
    }

    const size_t m_event_line_index = m_current_line_index;
    // Account for the "Event <N>" header line consumed by ReadNextEventLines
    m_current_line_index++;

    if (lines.size() == 1) {
        // Track header present but no hits — skip silently
        m_log->debug("Empty event (no hits) near line {}", m_current_line_index);
        m_current_line_index++;
        return Result::FailureTryAgain;
    }

    // -----------------------------------------------------------------------
    // Parse into intermediate POD structures
    // -----------------------------------------------------------------------
    DigitizedReadoutTrack track{};
    try {
        // First line: track header
        auto tokens = SplitDataString(lines[0]);
        if (!ParseTrackHeader(tokens, track))
            return Result::FailureFinished;

        // Remaining lines: hits
        for (size_t i = 1; i < lines.size(); ++i) {
            DigitizedReadoutHit hit{};
            tokens = SplitDataString(lines[i]);
            if (ParseTrackHit(tokens, hit))
                track.hits.emplace_back(hit);
        }

        m_current_line_index += lines.size();

    } catch (...) {
        fmt::print("Error parsing event/track near line {}\n", m_current_line_index);
        throw;
    }

    if (track.hits.empty()) {
        m_log->warn("No valid hits parsed near line {}", m_current_line_index);
        return Result::FailureFinished;
    }

    // Sort hits by time (ascending)
    std::sort(track.hits.begin(), track.hits.end(),
              [](const DigitizedReadoutHit& a, const DigitizedReadoutHit& b) {
                  return a.time < b.time;
              });

    // -----------------------------------------------------------------------
    // Copy to PODIO collections, converting units to ACTS natural units
    // -----------------------------------------------------------------------
    DigitizedMtpcMcTrackCollection podioTracks;
    DigitizedMtpcMcHitCollection   podioHits;

    auto podioTrack = podioTracks.create();
    podioTrack.setPhi(    track.phi      * Acts::UnitConstants::degree);
    podioTrack.setTheta(  track.theta    * Acts::UnitConstants::degree);
    podioTrack.setVertexZ(track.vertexZ  * Acts::UnitConstants::m);
    podioTrack.setMomentum(track.momentum * Acts::UnitConstants::GeV);

    for (const auto& hit : track.hits) {

        // Skip sentinel (out-of-acceptance) hits
        if (hit.ring == -999 || hit.ring == 999 ||
            hit.pad  == -999 || hit.pad  == 999)
        {
            m_log->warn(
                "Sentinel hit (ring={}, pad={}, plane={}) at event {} line {} — skipped",
                hit.ring, hit.pad, hit.plane,
                event.GetEventNumber(), m_event_line_index);
            continue;
        }

        auto podioHit = podioHits.create();
        podioHit.setTime(   hit.time   * Acts::UnitConstants::ns);
        podioHit.setAdc(    hit.adc);
        podioHit.setRing(   hit.ring);
        podioHit.setPad(    hit.pad);
        podioHit.setPlane(  hit.plane);
        podioHit.setZToGem( hit.zToGem * Acts::UnitConstants::m);

        auto [padX, padY] = getPadCenter(hit.ring, hit.pad);
        podioHit.setPadCenterX(padX);
        podioHit.setPadCenterY(padY);

        tdis::Vector3f truePos{
            static_cast<float>(hit.true_x * Acts::UnitConstants::m),
            static_cast<float>(hit.true_y * Acts::UnitConstants::m),
            static_cast<float>(hit.true_z * Acts::UnitConstants::m)
        };
        podioHit.setTruePosition(truePos);
        podioTrack.addToHits(podioHit);
    }

    EventInfoCollection info;
    info.push_back(MutableEventInfo(0, 0, 0));
    event.InsertCollection<EventInfo>(            std::move(info),        "EventInfo");
    event.InsertCollection<DigitizedMtpcMcTrack>( std::move(podioTracks), "DigitizedMtpcMcTracks");
    event.InsertCollection<DigitizedMtpcMcHit>(   std::move(podioHits),   "DigitizedMtpcMcHits");

    m_log->debug("Event {} read from file starting at line {}",
                 event.GetEventNumber(), m_event_line_index);
    return Result::Success;
}

inline std::string DigitizedDataEventSource::GetDescription() {
    return "Digitised TDIS MTPC text-file event source";
}

} // namespace tdis::io


// ---------------------------------------------------------------------------
// CheckOpenable specialisation (global namespace required by JANA2)
// ---------------------------------------------------------------------------
template <>
inline double JEventSourceGeneratorT<tdis::io::DigitizedDataEventSource>::CheckOpenable(
    std::string resource_name)
{
    return resource_name.ends_with("txt") ? 1.0 : 0.0;
}
