// Copyright 2022, David Lawrence
// Subject to the terms in the LICENSE file found in the top-level directory.
//
//

#include "ActsGeometryService.h"

#include <JANA/JException.h>
#include <TGeoManager.h>
#include <TGeoTube.h>
#include <extensions/spdlog/SpdlogToActs.h>
#include <fmt/ostream.h>

#include <Acts/Definitions/Units.hpp>
#include <Acts/Plugins/Json/JsonMaterialDecorator.hpp>
#include <Acts/Plugins/Json/MaterialMapJsonConverter.hpp>
#include <Acts/Visualization/GeometryView3D.hpp>
#include <Acts/Visualization/ObjVisualization3D.hpp>
#include <Acts/Visualization/PlyVisualization3D.hpp>
#include <Acts/Visualization/ViewConfig.hpp>
#include <array>
#include <exception>
#include <string>
#include <string_view>

#include "BuildCylindricalDetector.h"
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


namespace {
    // Function to recursively find the node by volume name
    inline TGeoNode* findNodeRecursive(TGeoNode* currentNode, const char* volumeName) {

        // Check if the current node's volume matches the name
        if (std::strcmp(currentNode->GetVolume()->GetName(), volumeName) == 0) {
            return currentNode;
        }
        // Recursively search in the daughter nodes
        int nDaughters = currentNode->GetNdaughters();
        for (int i = 0; i < nDaughters; ++i) {
            TGeoNode* daughterNode = currentNode->GetDaughter(i);
            TGeoNode* foundNode = findNodeRecursive(daughterNode, volumeName);
            if (foundNode != nullptr) {
                return foundNode;
            }
        }
        return nullptr;  // Not found in this branch
    }



    void printNodeTree(TGeoNode* currentNode, bool printVolumes = false, int level = 0) {
        // Print spaces corresponding to the level
        for (int i = 0; i < level; ++i) {
            std::cout << "   "; // Two spaces per level
        }

        // Print the node's name or volume's name
        if (printVolumes) {
            std::cout << currentNode->GetVolume()->GetName() << std::endl;
        } else {
            std::cout << currentNode->GetName() << std::endl;
        }

        // Recursively print daughter nodes
        int nDaughters = currentNode->GetNdaughters();
        for (int i = 0; i < nDaughters; ++i) {
            TGeoNode* daughterNode = currentNode->GetDaughter(i);
            printNodeTree(daughterNode, printVolumes, level + 1);
        }
    }

    /**
     *
     */
    void findNodesWithPrefix(TGeoNode* currentNode, const std::string& prefix, std::vector<TGeoNode*>& results, bool searchVolumes = false) {
        std::string_view name = searchVolumes ? currentNode->GetVolume()->GetName()
                                              : currentNode->GetName();

        if (name.starts_with(prefix)) {
            results.push_back(currentNode);
        }

        // Recursively search in the daughter nodes
        int nDaughters = currentNode->GetNdaughters();
        for (int i = 0; i < nDaughters; ++i) {
            TGeoNode* daughterNode = currentNode->GetDaughter(i);
            findNodesWithPrefix(daughterNode, prefix, results, searchVolumes);
        }
    }

    double roundTo2DecimalPlaces(double value) {
        return std::round(value * 100.0) / 100.0;
    }


#include <iostream>
#include <string>
#include "TGeoManager.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include "TGeoShape.h"
#include "TGeoMatrix.h"

    // Recursive function to find the node and compute its global matrix
    bool findNodeGlobalMatrix(TGeoNode* currentNode, TGeoNode* targetNode, TGeoHMatrix& globalMatrix) {
        if (!currentNode) {
            return false;
        }

        // Save the current global matrix
        TGeoHMatrix savedMatrix = globalMatrix;

        // Get the local transformation matrix of the current node
        TGeoMatrix* localMatrix = currentNode->GetMatrix();
        if (localMatrix) {
            // Multiply the global matrix by the local matrix
            globalMatrix.Multiply(localMatrix);
        }

        // Check if currentNode is the targetNode
        if (currentNode == targetNode) {
            // Found the node
            return true;
        }

        // Recursively search in daughter nodes
        int nDaughters = currentNode->GetNdaughters();
        for (int i = 0; i < nDaughters; ++i) {
            TGeoNode* daughterNode = currentNode->GetDaughter(i);

            // Create a copy of the current global matrix for the daughter
            TGeoHMatrix daughterGlobalMatrix = globalMatrix;

            if (findNodeGlobalMatrix(daughterNode, targetNode, daughterGlobalMatrix)) {
                // If the node is found in this branch, update the global matrix
                globalMatrix = daughterGlobalMatrix;
                return true;
            }
        }

        // Node not found in this branch, restore the global matrix
        globalMatrix = savedMatrix;
        return false;
    }

    std::tuple<double, double, double> getGlobalPosition(TGeoNode* node) {
        // Initialize the global matrix as identity
        TGeoHMatrix globalMatrix;

        // Start from the top node
        TGeoNode* topNode = gGeoManager->GetTopNode();


        // Find the node and compute its global matrix
        if (findNodeGlobalMatrix(topNode, node, globalMatrix)) {
            // Extract the translation (position) from the global matrix
            const Double_t* translation = globalMatrix.GetTranslation();

            // Print the global position
            return {translation[0], translation[1], translation[2]};
        }

        auto message = fmt::format("Node {} not found in the geometry tree.", node->GetName());
        throw std::runtime_error(message);
    }

    void printNodeGlobalPosition(TGeoNode* node) {
        if (!node) {
            std::cerr << "Invalid node provided." << std::endl;
            return;
        }

        // Initialize the global matrix as identity
        TGeoHMatrix globalMatrix;

        // Start from the top node
        TGeoNode* topNode = gGeoManager->GetTopNode();
        if (!topNode) {
            std::cerr << "Top node not found in the geometry manager." << std::endl;
            return;
        }

        // Find the node and compute its global matrix
        if (findNodeGlobalMatrix(topNode, node, globalMatrix)) {
            // Extract the translation (position) from the global matrix
            const Double_t* translation = globalMatrix.GetTranslation();

            // Print the global position
            std::cout << "Global position of the node '" << node->GetName() << "': ("
                      << translation[0] << ", "
                      << translation[1] << ", "
                      << translation[2] << ")" << std::endl;
        } else {
            std::cerr << "Node not found in the geometry tree." << std::endl;
        }
    }

    void printNodeInfo(TGeoNode* node) {
        if (!node) {
            std::cerr << "Invalid node provided." << std::endl;
            return;
        }

        // Get the navigator
        TGeoNavigator* navigator = gGeoManager->GetCurrentNavigator();
        if (!navigator) {
            // Create a navigator if one doesn't exist
            navigator = gGeoManager->AddNavigator();
        }

        // Save the current position
        TString currentPath = navigator->GetPath();

        // Get the volume and shape associated with the node
        TGeoVolume* volume = node->GetVolume();
        TGeoShape* shape = volume->GetShape();

        // Get and print the shape type
        std::string shapeType = shape->ClassName();
        std::cout << "Shape Type: " << shapeType << std::endl;

        // Retrieve and print dimensions based on the shape type
        if (shapeType == "TGeoTube" || shapeType == "TGeoTubeSeg") {
            TGeoTube* tube = dynamic_cast<TGeoTube*>(shape);
            if (tube) {
                std::cout << "Inner Radius (Rmin): " << tube->GetRmin() << std::endl;
                std::cout << "Outer Radius (Rmax): " << tube->GetRmax() << std::endl;
                std::cout << "Half Length Z (Dz): " << tube->GetDz() << std::endl;
            }
        }
        // Handle other shape types as needed

        // Restore the previous position
        navigator->cd(currentPath.Data());
    }
}


void tdis::tracking::ActsGeometryService::Init() {

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
    auto acts_init_log_level = tdis::SpdlogToActsLevel(m_init_log->level());

    // Load ACTS materials maps
    std::shared_ptr<const Acts::IMaterialDecorator> materialDeco{nullptr};
    if (!m_material_map_file().empty()) {
        m_init_log->info("loading materials map from file: '{}'", m_material_map_file());
        // Set up the converter first
        Acts::MaterialMapJsonConverter::Config jsonGeoConvConfig;
        // Set up the json-based decorator
        try {
            materialDeco = std::make_shared<const Acts::JsonMaterialDecorator>(jsonGeoConvConfig, m_material_map_file(), acts_init_log_level);
        }
        catch (const std::exception& e) {
            m_init_log->error("Failed to load materials map: {}", e.what());
            exit(1);    // TODO this is due to JANA2 issue #381. Remove after is fixed
        }
    }

    // Convert DD4hep geometry to ACTS
    m_init_log->info("Converting TGeo geometry to ACTS...");
    auto logger = tdis::makeActsLogger("CONV", m_log);

    m_log->info("Loading geometry file: ");
    m_log->info("   '{}'", m_tgeo_file());

    m_tgeo_manager = TGeoManager::Import(m_tgeo_file().c_str());
    if(!m_tgeo_manager) {
        m_init_log->error("Failed to load GeometryFile: TGeoManager::Import returned NULL");
        exit(1);    // TODO this is due to JANA2 issue #381. Remove after is fixed
    }

    if(!m_tgeo_manager->GetTopNode()) {
        m_init_log->error("Failed to load GeometryFile: TGeoManager::GetTopNode returned NULL");
        exit(1);    // TODO this is due to JANA2 issue #381. Remove after is fixed
    }

    if(!m_tgeo_manager->GetTopVolume()) {
        m_init_log->error("Failed to load GeometryFile: TGeoManager::GetTopVolume returned NULL");
        exit(1);    // TODO this is due to JANA2 issue #381. Remove after is fixed
    }

    if (m_log->level() <= (int) spdlog::level::debug) {
        printNodeTree(m_tgeo_manager->GetTopNode(), /*printVolumes*/ false);
    }

    std::vector<TGeoNode*> disk_nodes;
    findNodesWithPrefix(m_tgeo_manager->GetTopNode(), "mTPCReadoutDisc", disk_nodes);

    m_plane_positions.clear();
    std::vector<double> stereos;
    std::array<double, 2> offsets{{0, 0}};
    std::array<double, 2> bounds{{25, 100}};
    double thickness{0.1};

    m_init_log->info("Found readout disks: {}", disk_nodes.size());
    for (auto node : disk_nodes) {

        auto [x,y,z] = getGlobalPosition(node);
        // Get the volume and shape associated with the node
        TGeoVolume* volume = node->GetVolume();
        TGeoShape* shape = volume->GetShape();
        std::string shapeType = shape->ClassName();

        // Check it is TGeoTube as expected
        if (shapeType != "TGeoTube") {
            m_init_log->warn("For TGeoNode('{}') TGeoVolume ('{}') the shape not a TGeoTube as expected but: ", node->GetName(), volume->GetName(), shapeType);
            m_init_log->warn("We consider this as recoverable, but it might be if all readout disks are defined with other shape or something. Check this!");
            continue;
        }

        auto* tube = dynamic_cast<TGeoTube*>(shape);
        double r_min = tube->GetRmin();
        double r_max = tube->GetRmax();
        double dz = tube->GetDz();

        // We just copy it, considering the disks are the same
        bounds[0] = r_min * Acts::UnitConstants::cm;
        bounds[1] = r_max * Acts::UnitConstants::cm;
        thickness = dz * Acts::UnitConstants::cm * 0.001;

        m_log->info(fmt::format("   Position: ({:.4f}, {:.4f}, {:>8.4f}) cm, rmin: {:.3f} cm, rmax: {:.3f} cm, dz: {:.4f} cm, Node: '{}', Volume: '{}'",
                            x, y, z, r_min, r_max, dz, node->GetName(), volume->GetName()));

        m_plane_positions.push_back(z * Acts::UnitConstants::cm);
        stereos.push_back(0);
    }

    m_init_log->info("Building ACTS Geometry");

    /// Return the telescope detector
    // TODO remove it. This is from the old implementation
    // gGeometry = tdis::tracking::buildCylindricalDetector(
    //         nominalContext,             // gctx is the detector element dependent geometry context
    //         m_detector_elements,        // detectorStore is the store for the detector element
    //         m_plane_positions,          // positions are the positions of different layers in the longitudinal direction
    //         stereos,                    // stereoAngles are the stereo angles of different layers, which are rotation angles around the longitudinal (normal) direction
    //         offsets,                    // is the offset (u, v) of the layers in the transverse plane
    //         bounds,                     // bounds is the surface bound values, minR and maxR
    //         thickness,                  // thickness is the material thickness of each layer
    //         Acts::BinningValue::binZ);

    m_detector_cylinders.clear();

    gGeometry = tdis::tracking::buildCylindricalDetector(
        m_init_log,
        nominalContext,             // Geometry context
        m_detector_cylinders     // Detector element store
    );


    // Visualize ACTS geometry
    const Acts::TrackingVolume& tgVolume = *(gGeometry->highestTrackingVolume());

    // OBJ visualization export
    auto obj_file = m_obj_output_file();
    if(!m_obj_output_file().empty()) {
        m_log->info("ACTS exporting to OBJ: {}", m_obj_output_file());
        Acts::ObjVisualization3D vis_helper;
        DrawTrackingGeometry(vis_helper, tgVolume, m_geometry_context);
        vis_helper.write(m_obj_output_file());
    } else {
        m_log->info("ACTS OBJ Export. Flag '{}' is empty. NOT exporting.", m_obj_output_file.m_name);
    }

    // PLY visualization export
    if(!m_ply_output_file().empty()) {
        m_log->info("ACTS exporting to PLY: {}", m_ply_output_file());
        Acts::PlyVisualization3D vis_helper;
        DrawTrackingGeometry(vis_helper, tgVolume, m_geometry_context);
        vis_helper.write(m_ply_output_file());
    } else {
        m_log->info("ACTS PLY Export. Flag '{}' is empty. NOT exporting.", m_ply_output_file.m_name);
    }

    // Set ticker back
    m_app->SetTicker(was_ticker_enabled);

    m_init_log->info("ActsGeometryService initialization complete");
}

