// Copyright (C) 2025 Dmitry Romanov

#pragma once

#include <JANA/Components/JComponent.h>
#include <JANA/JApplication.h>
#include <JANA/Services/JServiceLocator.h>
#include <TGeoManager.h>
#include <spdlog/logger.h>

#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/Visualization/GeometryView3D.hpp>
#include <memory>
#include <mutex>

#include "MtpcDetectorElement.hpp"
#include "services/LogService.hpp"

namespace tdis::tracking {
    class ActsGeometryService : public JService {
      public:
        explicit ActsGeometryService() : JService() {}
        ~ActsGeometryService() override = default;

        void Init() override;

        TGeoManager* GetGeoManager() const { return m_tgeo_manager; }

        TGeoVolume* GetTopVolume() const { return m_tgeo_manager->GetTopVolume(); }

        TGeoNode* GetTopNode() const { return m_tgeo_manager->GetTopNode(); }

        Acts::GeometryContext& GetActsGeometryContext() { return m_geometry_context; }

        const std::vector<double>& GetPlanePositions() const { return m_plane_positions; }

        template <typename TVis> void DrawTrackingGeometry(TVis& vis_helper,
                                                           const Acts::TrackingVolume& tVolume,
                                                           const Acts::GeometryContext& gctx);

        /// Returns cylinder corresponding to ring index
        std::shared_ptr<tdis::tracking::MtpcDetectorElement> GetDetectorCylinder(size_t index) const {
            return m_detector_cylinders.at(index);
        }

        std::shared_ptr<const Acts::TrackingGeometry> GetTrackingGeometry() const {
            return gGeometry;
        }

      private:
        Parameter<std::string> m_tgeo_file{this, "acts:geometry", "g4sbs_mtpc.root","TGeo filename with geometry for ACTS"};
        Parameter<std::string> m_material_map_file{this, "acts:material_map", "", "JSON/CBOR material map file path"};
        Parameter<std::string> m_obj_output_file{this, "acts:output_obj", "", "Output file name to dump ACTS converted geometry as OBJ"};
        Parameter<std::string> m_ply_output_file{this, "acts:output_ply", "", "Output file name to dump ACTS converted geometry as PLY"};

        Service<tdis::services::LogService> m_service_log{this};

        // General acts log
        std::shared_ptr<spdlog::logger> m_log;

        /// Logger that is used for geometry initialization
        /// By default its level the same as ACTS general logger (m_log)
        /// But it might be customized to solely printout geometry information
        std::shared_ptr<spdlog::logger> m_init_log;

        /// Root TGeo Manater for TGeo Geometry
        TGeoManager* m_tgeo_manager = nullptr;

        Acts::GeometryContext m_geometry_context = Acts::GeometryContext();

        tdis::tracking::MtpcDetectorElement::ContextType nominalContext;

        // std::vector<std::shared_ptr<tdis::tracking::MtpcDetectorElement>> m_detector_elements;
        std::unordered_map<uint32_t, std::shared_ptr<MtpcDetectorElement>> m_detector_cylinders;

        std::shared_ptr<const Acts::TrackingGeometry> gGeometry;

        // Plane positions
        std::vector<double> m_plane_positions;
    };


    template <typename TVis>
    void ActsGeometryService::DrawTrackingGeometry(TVis& vis_helper, const Acts::TrackingVolume& tVolume, const Acts::GeometryContext& gctx) {
        bool triangulate = false;

        Acts::ViewConfig viewSensitive{.color = {0, 180, 240}, .quarterSegments = 72, .triangulate = triangulate};
        Acts::ViewConfig viewPassive{.color = {240, 280, 0}, .quarterSegments = 72, .triangulate = triangulate};
        Acts::ViewConfig viewVolume{.color = {220, 220, 0}, .quarterSegments = 72, .triangulate = triangulate};
        Acts::ViewConfig viewContainer{.color = {220, 220, 0}, .quarterSegments = 72, .triangulate = triangulate};
        Acts::ViewConfig viewGrid{.color = {220, 0, 0}, .offset = 3., .quarterSegments = 8, .triangulate = triangulate};

        Acts::GeometryView3D::drawTrackingVolume(
            vis_helper,  // Visualization helper (templated)
            tVolume,        // Tracking volume to be drawn
            gctx,           // Geometry context
            viewContainer,  // Container volume view configuration
            viewVolume,     // Navigation level volume view configuration
            viewPassive,    // Passive surfaces view configuration
            viewSensitive,  // Sensitive surfaces view configuration
            viewGrid,       // Grid display view configuration
            true,           // Write or not
            "tdis"          // Optional tag
        );

    }
}   // namespace tdis::tracking