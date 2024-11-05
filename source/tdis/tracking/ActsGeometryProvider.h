// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2015 - 2024 Julia Hrdinka, Whitney Armstrong, Wouter Deconinck, Dmitry Romanov

#pragma once

// ACTS
#include <Acts/Definitions/Units.hpp>
#include <Acts/Geometry/GeometryContext.hpp>
#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/MagneticField/MagneticFieldProvider.hpp>
#include <Acts/Surfaces/Surface.hpp>
#include <Acts/Utilities/Logger.hpp>
#include <Acts/Visualization/ViewConfig.hpp>
#include <Math/GenVector/Cartesian3D.h>
#include <Math/GenVector/DisplacementVector3D.h>
#include <spdlog/logger.h>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "DD4hepBField.h"

namespace dd4hep::rec {
    class Surface;
}

/** Draw the surfaces and save to obj file.
 *  This is useful for debugging the ACTS geometry. The obj file can
 *  be loaded into various tools, such as FreeCAD, for inspection.
 */
void draw_surfaces(std::shared_ptr<const Acts::TrackingGeometry> trk_geo, std::shared_ptr<spdlog::logger> init_log, const std::string &fname);

class ActsGeometryProvider {
public:
    ActsGeometryProvider() {}
    using VolumeSurfaceMap = std::unordered_map<uint64_t, const Acts::Surface *>;

    virtual void initialize(const std::string& geometry_file,
                            const std::string& material_file,
                            std::shared_ptr<spdlog::logger> log,
                            std::shared_ptr<spdlog::logger> init_log) final;


    /** Gets the ACTS tracking geometry.
     */
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry() const { return m_trackingGeo;}

    std::shared_ptr<const Acts::MagneticFieldProvider> getFieldProvider() const  { return m_magneticField; }


    const VolumeSurfaceMap &surfaceMap() const  { return m_surfaces; }


    std::map<int64_t, dd4hep::rec::Surface *> getDD4hepSurfaceMap() const { return m_surfaceMap; }

    const Acts::GeometryContext& getActsGeometryContext() const {return m_trackingGeoCtx;}

    ///  ACTS general logger that is used for running ACTS
    std::shared_ptr<spdlog::logger> getActsRelatedLogger() const { return m_log; }

    /// Logger that is used for geometry initialization
    /// By default its level the same as ACTS general logger (m_log)
    /// But it might be customized to solely printout geometry information
    std::shared_ptr<spdlog::logger> getActsInitRelatedLogger()  const { return m_init_log; }

private:


    /// DD4hep surface map
    std::map<int64_t, dd4hep::rec::Surface *> m_surfaceMap;

    /// ACTS Logging Level
    Acts::Logging::Level acts_log_level = Acts::Logging::INFO;

    /// ACTS Tracking Geometry Context
    Acts::GeometryContext m_trackingGeoCtx;

    /// ACTS Tracking Geometry
    std::shared_ptr<const Acts::TrackingGeometry> m_trackingGeo{nullptr};

    /// ACTS surface lookup container for hit surfaces that generate smeared hits
    VolumeSurfaceMap m_surfaces;

    /// Acts magnetic field
    std::shared_ptr<const eicrecon::BField::DD4hepBField> m_magneticField = nullptr;

    ///  ACTS general logger that is used for running ACTS
    std::shared_ptr<spdlog::logger> m_log;

    /// Logger that is used for geometry initialization
    /// By default its level the same as ACTS general logger (m_log)
    /// But it might be customized to solely printout geometry information
    std::shared_ptr<spdlog::logger> m_init_log;

    /// Configuration for obj export
    Acts::ViewConfig m_containerView{true, {220, 220, 220}};
    Acts::ViewConfig m_volumeView{true, {220, 220, 0}};
    Acts::ViewConfig m_sensitiveView{true, {0, 180, 240}};
    Acts::ViewConfig m_passiveView{true, {240, 280, 0}};
    Acts::ViewConfig m_gridView{true, {220, 0, 0}};
    bool m_objWriteIt{false};
    bool m_plyWriteIt{false};
    std::string m_outputTag{""};
    std::string m_outputDir{""};

public:
    void setObjWriteIt(bool writeit) { m_objWriteIt = writeit; }
    bool getObjWriteIt() const { return m_objWriteIt; }
    void setPlyWriteIt(bool writeit) { m_plyWriteIt = writeit; }
    bool getPlyWriteIt() const { return m_plyWriteIt; }

    void setOutputTag(std::string tag) { m_outputTag = tag; }
    std::string getOutputTag() const { return m_outputTag; }
    void setOutputDir(std::string dir) { m_outputDir = dir; }
    std::string getOutputDir() const { return m_outputDir; }

    void setContainerView(std::array<int,3> view) {
        m_containerView.color = Acts::Color{view};
    }
    const Acts::ViewConfig& getContainerView() const { return m_containerView; }
    void setVolumeView(std::array<int,3> view) {
        m_volumeView.color = Acts::Color{view};
    }
    const Acts::ViewConfig& getVolumeView() const { return m_volumeView; }
    void setSensitiveView(std::array<int,3> view) {
        m_sensitiveView.color = Acts::Color{view};
    }
    const Acts::ViewConfig& getSensitiveView() const { return m_sensitiveView; }
    void setPassiveView(std::array<int,3> view) {
        m_passiveView.color = Acts::Color{view};
    }

    const Acts::ViewConfig& getPassiveView() const { return m_passiveView; }
    void setGridView(std::array<int,3> view) {
        m_gridView.color = Acts::Color{view};
    }
    const Acts::ViewConfig& getGridView() const { return m_gridView; }

};