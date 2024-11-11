// Copyright 2022, Dmitry Romanov
// Subject to the terms in the LICENSE file found in the top-level directory.

/**
 * LogService provides a centralized way to spawn spdlog loggers with known parameters
 */

#pragma once

#include <JANA/JApplication.h>
#include <JANA/Services/JServiceLocator.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <JANA/JException.h>
#include <spdlog/spdlog.h>
#include <exception>


namespace tdis::services {

    /// Convert string to spdlog::level::level_enum
    inline spdlog::level::level_enum ParseLogLevel(const std::string &input) {

        // Convert the source string to lower case
        std::string lc_input;              // Lower case input
        lc_input.resize(input.size());
        std::transform(input.begin(), input.end(), lc_input.begin(), ::tolower);

        if(lc_input == "trace" || lc_input == std::to_string(SPDLOG_LEVEL_TRACE)) return spdlog::level::trace;
        if(lc_input == "debug" || lc_input == std::to_string(SPDLOG_LEVEL_DEBUG)) return spdlog::level::debug;
        if(lc_input == "info" || lc_input == std::to_string(SPDLOG_LEVEL_INFO)) return spdlog::level::info;
        if(lc_input == "warn" || lc_input == "warning" || lc_input == std::to_string(SPDLOG_LEVEL_WARN)) return spdlog::level::warn;
        if(lc_input == "err" || lc_input == "error" || lc_input == std::to_string(SPDLOG_LEVEL_ERROR)) return spdlog::level::err;
        if(lc_input == "critical" || lc_input == std::to_string(SPDLOG_LEVEL_CRITICAL)) return spdlog::level::critical;
        if(lc_input == "off" || lc_input == std::to_string(SPDLOG_LEVEL_OFF)) return spdlog::level::off;

        auto err_msg = fmt::format("ParseLogLevel can't parse input string: '{}'", input);
        throw JException(err_msg);
    }

    /// Converts spdlog::level::level_enum to std::string
    inline std::string LogLevelToString(spdlog::level::level_enum input) {

        // Convert the source string to lower case
        switch (input) {
            case spdlog::level::trace:
                return "trace";
            case spdlog::level::debug:
                return "debug";
            case spdlog::level::info:
                return "info";
            case spdlog::level::warn:
                return "warn";
            case spdlog::level::err:
                return "error";
            case spdlog::level::critical:
                return "critical";
            case spdlog::level::off:
                return "off";
            case spdlog::level::n_levels:
                [[fallthrough]];
            default:
                break;
        }

        auto err_msg = fmt::format("ParseLogLevel don't know this log level: '{}'", fmt::underlying(input));
        throw JException(err_msg);
    }

    /// LogService provides centralized way of using spdlog and controlling them with flags
    class LogService : public JService {

        LogService() = default;

        std::recursive_mutex m_lock;
        JApplication* m_application;
        std::string m_log_level_str;
        std::string m_log_format_str;
    public:
        using level = spdlog::level::level_enum;

        explicit LogService(JApplication *app): m_application(app){

            m_log_level_str = "info";
            m_application->SetDefaultParameter("tdis:LogLevel", m_log_level_str, "log_level: trace, debug, info, warn, error, critical, off");
            spdlog::default_logger()->set_level(ParseLogLevel(m_log_level_str));

            m_log_format_str = "[%n] [%^%l%$] %v";
            m_application->SetDefaultParameter("tdis:LogFormat", m_log_level_str, "spdlog pattern string");
            spdlog::set_pattern(m_log_format_str);
        }


        /** Get a named logger with optional level
         * When no level is specified, the service default is used **/
        std::shared_ptr<spdlog::logger> logger(const std::string &name, const std::optional<spdlog::level::level_enum> default_level = std::nullopt) {
            try {
                std::lock_guard<std::recursive_mutex> locker(m_lock);

                // Try to get existing logger
                auto logger = spdlog::get(name);
                if (!logger) {
                    // or create a new one with current configuration
                    logger = spdlog::default_logger()->clone(name);

                    // Set log level for this named logger allowing user to specify as config. parameter
                    // e.g. EcalEndcapPRecHits:LogLevel
                    std::string log_level_str = default_level ? LogLevelToString(default_level.value()) : m_log_level_str;
                    m_application->SetDefaultParameter(name + ":log_level", log_level_str, "log_level for " + name + ": trace, debug, info, warn, error, critical, off");
                    logger->set_level(ParseLogLevel(log_level_str));
                }
                return logger;
            }
            catch (const std::exception & exception) {
                throw JException(exception.what());
            }
        }

        /** Gets the default level for all loggers
         * The log level is set from user parameters or is 'info' **/
        spdlog::level::level_enum getDefaultLevel() {
            return spdlog::default_logger()->level();
        }

        /** Gets std::string version of the default log level **/
        std::string getDefaultLevelStr() {
            return LogLevelToString(getDefaultLevel());
        }
    };
}   // namespace tdis::services