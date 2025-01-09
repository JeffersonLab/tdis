// This file is part of the Acts project.
//
// Copyright (C) 2020-2021 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <cstddef>
#include <utility>

#include "Acts/Definitions/Algebra.hpp"
#include "Acts/Definitions/Units.hpp"
#include "Acts/Geometry/CylinderVolumeBounds.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Geometry/ILayerArrayCreator.hpp"
#include "Acts/Geometry/ITrackingVolumeHelper.hpp"
#include "Acts/Geometry/LayerArrayCreator.hpp"
#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/Geometry/TrackingVolume.hpp"
#include <Acts/Geometry/CylinderLayer.hpp>
#include "Acts/Material/HomogeneousVolumeMaterial.hpp"
#include "Acts/Material/Material.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Surfaces/SurfaceArray.hpp"
#include "Acts/Utilities/Logger.hpp"
#include "BuildMtpcDetectorGEM.hpp"

#include <memory>
#include <vector>

/** This geometry uses cylindrical surfaces with:
 *    - R (radius) of each cylinder - corresponding to each of mTPC ring center
 *    - The length of each cylinder in z direction equal to whole detector length
 *  This way the finding a point on a plane for ACTS Kalman filtering is easy in terms of Z coordinate
 *  Then each cylinder correspond to MtpcDetectorElement in detectorStore
 */
std::unique_ptr<const Acts::TrackingGeometry>
tdis::tracking::buildCylindricalDetectorGEM(
    const typename MtpcDetectorElement::ContextType& gctx,
    std::unordered_map<uint32_t, std::shared_ptr<MtpcDetectorElement>>& surfaceStore)
{
    using namespace Acts;
    using namespace Acts::UnitLiterals;

    // Define Argon gas material properties at STP
    double radiationLength = 19.55_m;       // Radiation length in mm (19.55 m)
    double interactionLength = 70.0_m;      // Interaction length in mm (70 m)
    double atomicMass = 39.948;             // Atomic mass of Argon
    double atomicNumber = 18;               // Atomic number of Argon
    double massDensity = 1.66e-6_g / 1_mm3; // Mass density in g/mm³

    // Create Argon gas material
    Material argonGas = Material::fromMassDensity(
        radiationLength, interactionLength, atomicMass, atomicNumber, massDensity);

    // Define constants
    double innerRadius = 50_mm;     // 5 cm in mm
    double outerRadius = 150_mm;    // 15 cm in mm
    double cylinderLength = 550_mm; // 55 cm in mm
    double halfLength = cylinderLength / 2.0; // Half-length of the cylinder
    int numRings = 21;              // Number of concentric rings
    double radialStep = (outerRadius - innerRadius) / numRings;

    // Create vector to hold cylinder layers
    std::vector<std::shared_ptr<const Layer>> cylinderLayers;

    for (int i = 0; i < numRings; ++i) {
        // Calculate the radius of the current ring (centered within its radial step)
        double radius = innerRadius + (i + 0.5) * radialStep;

        // Define the cylinder bounds
        auto cylinderBounds = std::make_shared<const CylinderBounds>(radius, halfLength);

        // Create the detector element
        uint32_t id = static_cast<uint32_t>(i); // Ring index, 0 for innermost

        auto elementTransform = std::make_shared<const Transform3>(Transform3::Identity());

        // Create the detector element
        auto detElem = std::make_shared<MtpcDetectorElement>(
            id,
            elementTransform,
            cylinderBounds,
            radialStep // Thickness of the layer
            // No surface material assigned
        );

        // Store the detector element
        surfaceStore[id] = detElem;

        // Get the surface from the detector element
        auto& cylinderSurface = detElem->surface();

        // Create a vector of surfaces
        std::vector<std::shared_ptr<const Surface>> surfaces;
        surfaces.push_back(cylinderSurface);

        // Create the SurfaceArray
        auto surfaceArray = std::make_shared<SurfaceArray>(
            Transform3(Vector3(0, 0, 0)), // Center at origin
            std::move(surfaces));

        // Now create the CylinderLayer using the create method
        auto cylinderLayer = CylinderLayer::create(
            Transform3::Identity(),        // Transform
            cylinderBounds,                // Cylinder bounds
            surfaceArray,                  // Surface array
            radialStep,                    // Layer thickness
            nullptr,                       // No approach descriptor
            LayerType::active);            // Layer type

        // Add the layer to the vector
        cylinderLayers.push_back(cylinderLayer);
    }

    // Create a LayerArray from the cylinder layers
    Acts::LayerArrayCreator::Config layerArrayCreatorConfig;
    LayerArrayCreator layerArrayCreator(layerArrayCreatorConfig);
    auto layerArray = layerArrayCreator.layerArray(
        gctx,
        cylinderLayers,
        innerRadius,
        outerRadius,
        BinningType::arbitrary,
        BinningValue::binR);

    // Define the cylinder volume bounds (outermost dimensions)
    auto volumeBounds = std::make_shared<CylinderVolumeBounds>(
        innerRadius, outerRadius, halfLength);

    // Create the material for the volume
    auto volumeMaterial = std::make_shared<HomogeneousVolumeMaterial>(argonGas);

    // Create the tracking volume with the layers
    auto trackingVolume = std::make_shared<TrackingVolume>(
        Transform3::Identity(),      // No transformation (centered at origin)
        volumeBounds,                // Volume bounds
        volumeMaterial,              // Volume material
        std::move(layerArray),       // Layer array
        nullptr,                     // No contained volumes
        MutableTrackingVolumeVector{},
        "TPCVolume");

    // Create the TrackingGeometry with the tracking volume as the world volume
    auto trackingGeometry = std::make_unique<TrackingGeometry>(trackingVolume);

    return trackingGeometry;
}