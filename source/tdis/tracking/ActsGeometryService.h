// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck, Dmitry Romanov

#pragma once

#include <JANA/Components/JComponent.h>
#include <JANA/JApplication.h>
#include <JANA/Services/JServiceLocator.h>
#include <TGeoManager.h>
#include <spdlog/logger.h>

#include "TelescopeDetectorElement.hpp"

#include <memory>
#include <mutex>

#include "ActsGeometryProvider.h"
#include "services/LogService.hpp"

namespace tdis::tracking {
    class ActsGeometryService : public JService
    {
    public:
        explicit ActsGeometryService() : JService() {}
        ~ActsGeometryService() override = default;


        void Init() override;

        TGeoManager* GetGeoManager() const { return m_tgeo_manager; }

        TGeoVolume * GetTopVolume() const { return m_tgeo_manager->GetTopVolume(); }

        TGeoNode * GetTopNode() const { return m_tgeo_manager->GetTopNode(); }


        Acts::GeometryContext& GetActsGeometryContext() {
            return m_geometry_context;
        }


    private:


        Parameter<std::string> m_tgeo_file {this, "acts:geometry", "g4sbs_mtpc.root", "TGeo filename with geometry for ACTS"};
        Parameter<std::string> m_material_map_file {this, "acts:material_map", "", "JSON/CBOR material map file path"};
        Parameter<std::string> m_obj_output_file {this, "acts:output_obj", "", "Output file name to dump ACTS converted geometry as OBJ"};
        Parameter<std::string> m_ply_output_file {this, "acts:output_ply", "", "Output file name to dump ACTS converted geometry as PLY"};

        Service<tdis::services::LogService> m_service_log {this};


        // General acts log
        std::shared_ptr<spdlog::logger> m_log;

        /// Logger that is used for geometry initialization
        /// By default its level the same as ACTS general logger (m_log)
        /// But it might be customized to solely printout geometry information
        std::shared_ptr<spdlog::logger> m_init_log;

        /// Root TGeo Manater for TGeo Geometry
        TGeoManager* m_tgeo_manager;

        Acts::GeometryContext m_geometry_context = Acts::GeometryContext();

        ActsExamples::Telescope::TelescopeDetectorElement::ContextType nominalContext;

        std::vector<std::shared_ptr<ActsExamples::Telescope::TelescopeDetectorElement>> detectorStore;

    };
}   // namespace tdis::tracking