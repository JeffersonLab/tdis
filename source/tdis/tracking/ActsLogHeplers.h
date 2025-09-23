//
// Created by Dmitry Romanov on 7/1/2025.
// Subject to the terms in the LICENSE file found in the top-level directory.
//

#pragma once
#include <Acts/Utilities/Logger.hpp>

inline Acts::Logging::Level strToActsLevel(std::string_view lvl) {
    using namespace Acts::Logging;
    if (lvl == "VERBOSE" || lvl == "verbose") return Level::VERBOSE;
    if (lvl == "DEBUG"|| lvl == "debug")      return Level::DEBUG;
    if (lvl == "INFO" || lvl == "info")       return Level::INFO;
    if (lvl == "WARNING" || lvl == "warning") return Level::WARNING;
    if (lvl == "ERROR" || lvl == "error")     return Level::ERROR;
    if (lvl == "FATAL" || lvl == "fatal")     return Level::FATAL;

    // Oops! Unknown log level Form and error:
    auto message = "Invalid logging level: " + std::string(lvl) + "'";
    throw std::invalid_argument(message);
}


