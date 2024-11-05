#pragma once

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
#include <TGeoManager.h>
#include <unordered_map>


namespace tdis::acts {
    /** Draw the surfaces and save to obj file.
     *  This is useful for debugging the ACTS geometry. The obj file can
     *  be loaded into various tools, such as FreeCAD, for inspection.
     */
    void draw_surfaces(std::shared_ptr<const Acts::TrackingGeometry> trk_geo, std::shared_ptr<spdlog::logger> init_log, const std::string &fname);


    class ActsGeometryProvider {
    public:

        void InitializeGeometry(const std::string& path) {
            TGeoManager::Import(path.c_str());

        }
    };
}


