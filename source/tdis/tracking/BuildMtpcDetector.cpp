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
#include "Acts/Geometry/CuboidVolumeBounds.hpp"
#include "Acts/Geometry/CylinderVolumeBounds.hpp"
#include "Acts/Geometry/DiscLayer.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Geometry/ILayerArrayCreator.hpp"
#include "Acts/Geometry/ITrackingVolumeHelper.hpp"
#include "Acts/Geometry/LayerArrayCreator.hpp"
#include "Acts/Geometry/PlaneLayer.hpp"
#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/Geometry/TrackingVolume.hpp"
#include <Acts/Geometry/CylinderLayer.hpp>
#include "Acts/Material/HomogeneousSurfaceMaterial.hpp"
#include "Acts/Material/HomogeneousVolumeMaterial.hpp"
#include "Acts/Material/Material.hpp"
#include "Acts/Material/MaterialSlab.hpp"
#include "Acts/Surfaces/RadialBounds.hpp"
#include "Acts/Surfaces/RectangleBounds.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Surfaces/SurfaceArray.hpp"
#include "Acts/Utilities/Logger.hpp"
#include "BuildMtpcDetector.hpp"


#include <memory>
#include <vector>
/*
static std::unique_ptr<const Acts::TrackingGeometry>
tdis::tracking::buildOriginalDetector(
    const typename tdis::tracking::MtpcDetectorElement::ContextType &gctx,
    std::vector<std::shared_ptr<tdis::tracking::MtpcDetectorElement> > &detectorStore,
    const std::vector<double> &positions,
    const std::vector<double> &stereoAngles,
    const std::array<double, 2> &offsets,
    const std::array<double, 2> &bounds,
    double thickness,
    Acts::BinningValue binValue)
{
    using namespace Acts::UnitLiterals;


    // The radial bounds for disc surface
    const auto rBounds = std::make_shared<const Acts::RadialBounds>(bounds[0], bounds[1]);

    // Material of the surfaces
    Acts::Material silicon = Acts::Material::fromMassDensity(9.370_cm, 46.52_cm, 28.0855, 14, 2.329_g / 1_cm3);
    Acts::MaterialSlab matProp(silicon, thickness);
    const auto surfaceMaterial = std::make_shared<Acts::HomogeneousSurfaceMaterial>(matProp);

    // Construct the rotation
    // This assumes the binValue is binX, binY or binZ. No reset is necessary in
    // case of binZ
    Acts::RotationMatrix3 rotation = Acts::RotationMatrix3::Identity();
    if (binValue == Acts::BinningValue::binX) {
        rotation.col(0) = Acts::Vector3(0, 0, -1);
        rotation.col(1) = Acts::Vector3(0, 1, 0);
        rotation.col(2) = Acts::Vector3(1, 0, 0);
    } else if (binValue == Acts::BinningValue::binY) {
        rotation.col(0) = Acts::Vector3(1, 0, 0);
        rotation.col(1) = Acts::Vector3(0, 0, -1);
        rotation.col(2) = Acts::Vector3(0, 1, 0);
    }

    // Construct the surfaces and layers
    std::size_t nLayers = positions.size();
    std::vector<Acts::LayerPtr> layers(nLayers);
    for (unsigned int i = 0; i < nLayers; ++i) {
        // The translation without rotation yet
        Acts::Translation3 trans(offsets[0], offsets[1], positions[i]);
        // The entire transformation (the coordinate system, whose center is defined
        // by trans, will be rotated as well)
        Acts::Transform3 trafo(rotation * trans);

        // rotate around local z axis by stereo angle
        auto stereo = stereoAngles[i];
        trafo *= Acts::AngleAxis3(stereo, Acts::Vector3::UnitZ());

        // Create the detector element
        std::shared_ptr<MtpcDetectorElement> detElement = nullptr;

        detElement = std::make_shared<MtpcDetectorElement>(std::make_shared<const Acts::Transform3>(trafo), rBounds, 1._um, i,  surfaceMaterial);

        // Get the surface
        auto surface = detElement->surface().getSharedPtr();

        // Add the detector element to the detector store
        detectorStore.push_back(std::move(detElement));
        // Construct the surface array (one surface contained)
        std::unique_ptr<Acts::SurfaceArray> surArray(new Acts::SurfaceArray(surface));
        // Construct the layer
        layers[i] = Acts::DiscLayer::create(trafo, rBounds, std::move(surArray), thickness);

        // Associate the layer to the surface
        auto mutableSurface = const_cast<Acts::Surface *>(surface.get());
        mutableSurface->associateLayer(*layers[i]);
    }

    // The volume transform
    Acts::Translation3 transVol(offsets[0], offsets[1], (positions.front() + positions.back()) * 0.5);
    Acts::Transform3 trafoVol(rotation * transVol);

    // The volume bounds is set to be a bit larger than either cubic with planes
    // or cylinder with discs
    auto length = positions.back() - positions.front();
    std::shared_ptr<Acts::VolumeBounds> boundsVol = nullptr;
    boundsVol = std::make_shared<Acts::CylinderVolumeBounds>(std::max(bounds[0] - 5.0_mm, 0.), bounds[1] + 5._mm, length + 10._mm);

    Acts::LayerArrayCreator::Config lacConfig;
    Acts::LayerArrayCreator layArrCreator(lacConfig, Acts::getDefaultLogger("LayerArrayCreator", Acts::Logging::INFO));
    Acts::LayerVector layVec;
    for (unsigned int i = 0; i < nLayers; i++) {
        layVec.push_back(layers[i]);
    }
    // Create the layer array
    Acts::GeometryContext genGctx{gctx};
    std::unique_ptr<const Acts::LayerArray> layArr(layArrCreator.layerArray(genGctx, layVec, positions.front() - 2._mm, positions.back() + 2._mm, Acts::BinningType::arbitrary, binValue));

    // Build the tracking volume
    auto trackVolume = std::make_shared<Acts::TrackingVolume>(trafoVol, boundsVol, nullptr, std::move(layArr), nullptr, Acts::MutableTrackingVolumeVector{}, "Telescope");

    // Build and return tracking geometry
    return std::make_unique<Acts::TrackingGeometry>(trackVolume);
}
*/

/** This geometry uses cylindrical surfaces with:
 *    - R (radius) of each cylinder - corresponding to each of mTPC ring center
 *    - The length of each cylinder in z direction equal to whole detector length
 *  This way the finding a point on a plane for ACTS Kalman filtering is easy in terms of Z coordinate
 *  Then each cylinder correspond to MtpcDetectorElement in detectorStore
 */
std::unique_ptr<const Acts::TrackingGeometry>
tdis::tracking::buildCylindricalDetector(
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
    std::vector<std::shared_ptr<const CylinderLayer>> cylinderLayers;

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
        auto cylinderSurface = detElem->surface().getSharedPtr();

        // Create a vector of surfaces
        std::vector<std::shared_ptr<const Surface>> surfaces;
        surfaces.push_back(cylinderSurface);

        // Create the SurfaceArray
        auto surfaceArray = std::make_unique<SurfaceArray>(
            Transform3(Vector3(0, 0, 0)), // Center at origin
            std::move(surfaces));

        // Now create the CylinderLayer using the create method
        auto cylinderLayer = CylinderLayer::create(
            Transform3::Identity(),        // Transform
            cylinderBounds,                // Cylinder bounds
            std::move(surfaceArray),       // Surface array
            radialStep,                    // Layer thickness
            nullptr,                       // No approach descriptor
            LayerType::sensitive);         // Layer type

        // Add the layer to the vector
        cylinderLayers.push_back(std::move(cylinderLayer));
    }

    // Create a LayerArray from the cylinder layers
    Acts::LayerArrayCreator::Config layerArrayCreatorConfig;
    LayerArrayCreator layerArrayCreator(layerArrayCreatorConfig);
    auto layerArray = layerArrayCreator.layerArray(
        GeometryContext(),
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




