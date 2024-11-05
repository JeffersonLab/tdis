// Copyright 2022, David Lawrence
// Subject to the terms in the LICENSE file found in the top-level directory.
//
//

#include "ActsGeometryService.h"

#include <JANA/JException.h>
#include <TGeoManager.h>
#include <extensions/spdlog/SpdlogToActs.h>
#include <fmt/ostream.h>

#include <Acts/Plugins/Json/JsonMaterialDecorator.hpp>
#include <Acts/Plugins/Json/MaterialMapJsonConverter.hpp>
#include <Acts/Visualization/ViewConfig.hpp>
#include <array>
#include <exception>
#include <string>

#include "ActsGeometryProvider.h"
#include "services/LogService.hpp"

// Formatter for Eigen matrices
#if FMT_VERSION >= 90000
#    include <Eigen/Core>

namespace Acts {
    class JsonMaterialDecorator;
}
template <typename T>
struct fmt::formatter<
    T,
    std::enable_if_t<
        std::is_base_of_v<Eigen::MatrixBase<T>, T>,
        char
    >
> : fmt::ostream_formatter {};
#endif // FMT_VERSION >= 90000


void ActsGeometryService::Init() {

    m_log = m_service_log->logger("acts");
    m_init_log = m_service_log->logger("acts_init");

    m_init_log->debug("ActsGeometryService is initializing...");

    m_init_log->debug("Set TGeoManager and acts_init_log_level log levels");
    // Turn off TGeo printouts if appropriate for the msg level
    if (m_log->level() >= (int) spdlog::level::info) {
        TGeoManager::SetVerboseLevel(0);
    }

    // Reading the geometry may take a long time and if the JANA ticker is enabled, it will keep printing
    // while no other output is coming which makes it look like something is wrong. Disable the ticker
    // while parsing and loading the geometry
    auto was_ticker_enabled = m_app->IsTickerEnabled();
    m_app->SetTicker(false);


    // Set ACTS logging level
    auto acts_init_log_level = eicrecon::SpdlogToActsLevel(m_init_log->level());

    // Load ACTS materials maps
    std::shared_ptr<const Acts::IMaterialDecorator> materialDeco{nullptr};
    if (!m_material_map_file().empty()) {
        m_init_log->info("loading materials map from file: '{}'", m_material_map_file());
        // Set up the converter first
        Acts::MaterialMapJsonConverter::Config jsonGeoConvConfig;
        // Set up the json-based decorator
        materialDeco = std::make_shared<const Acts::JsonMaterialDecorator>(jsonGeoConvConfig, m_material_map_file(), acts_init_log_level);
    }

    // Convert DD4hep geometry to ACTS
    m_init_log->info("Converting TGeo geometry to ACTS...");
    auto logger = eicrecon::getSpdlogLogger("CONV", m_log);



    // Set ticker back
    m_app->SetTicker(was_ticker_enabled);

    m_init_log->info("ActsGeometryService initialization complete");
}

