//
// Created by Dmitry Romanov on 7/1/2025.
// Subject to the terms in the LICENSE file found in the top-level directory.
//

#pragma once
#include <Acts/Utilities/Logger.hpp>

inline Acts::Logging::Level strToActsLevel(std::string_view lvl) {
    using L = Acts::Logging::Level;
    if      (lvl == "VERBOSE") return L::VERBOSE;
    else if (lvl == "DEBUG")   return L::DEBUG;
    else if (lvl == "INFO")    return L::INFO;
    else if (lvl == "WARNING") return L::WARNING;
    else if (lvl == "ERROR")   return L::ERROR;
    else if (lvl == "FATAL")   return L::FATAL;
    else                       return L::INFO;          // fallback
}


