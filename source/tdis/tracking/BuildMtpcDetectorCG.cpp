// This file is part of the Acts project.
//
// Copyright (C) 2020-2021 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "BuildMtpcDetector.hpp"

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Definitions/Units.hpp>
#include <Acts/Geometry/CylinderLayer.hpp>
#include <Acts/Geometry/CylinderVolumeBounds.hpp>
#include <Acts/Geometry/GeometryContext.hpp>
#include <Acts/Geometry/ILayerArrayCreator.hpp>
#include <Acts/Geometry/LayerArrayCreator.hpp>
#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/Geometry/TrackingVolume.hpp>
#include <Acts/Material/HomogeneousVolumeMaterial.hpp>
#include <Acts/Material/Material.hpp>
#include <Acts/Material/MaterialSlab.hpp>
#include <Acts/Surfaces/SurfaceArray.hpp>
#include <Acts/Utilities/BinningType.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <memory>
#include <vector>
#include <unordered_map>

#include "MtpcDetectorElement.hpp"

namespace tdis {
namespace tracking {

/** This geometry uses cylindrical surfaces with:
 *    - R (radius) of each cylinder - corresponding to each mTPC ring center
 *    - The length of each cylinder in Z direction equal to the entire detector length (55 cm)
 *  Then each cylinder corresponds to an MtpcDetectorElement in the detector store.
 */
std::unique_ptr<const Acts::TrackingGeometry>
buildCylindricalDetector(
    const MtpcDetectorElement::ContextType& gctx,
    std::unordered_map<uint32_t, std::shared_ptr<MtpcDetectorElement>>& surfaceStore)
{
  using namespace Acts;
  using namespace Acts::UnitLiterals;

  // Define Argon gas material properties at STP
  double radiationLength = 19.55_m;       // ~ 19.55 m in mm
  double interactionLength = 70.0_m;      // ~ 70.0 m in mm
  double atomicMass = 39.948;             // Argon
  double atomicNumber = 18;               // Argon
  double massDensity = 1.66e-6_g / 1_mm3; // g/mm^3

  // Create Argon gas material
  Material argonGas = Material::fromMassDensity(
      radiationLength, interactionLength, atomicMass, atomicNumber, massDensity);

  // Geometry parameters
  double innerRadius = 50_mm;     // 5 cm
  double outerRadius = 150_mm;    // 15 cm
  double cylinderLength = 550_mm; // 55 cm
  double halfLength = cylinderLength / 2.0;
  int numRings = 21;              // 21 concentric rings
  double radialStep = (outerRadius - innerRadius) / numRings;

  // We store the final layers as generic Layer pointers
  std::vector<std::shared_ptr<const Layer>> cylinderLayers;
  cylinderLayers.reserve(numRings);

  for (int i = 0; i < numRings; ++i) {
    // Calculate the radius of the current ring
    double radius = innerRadius + (i + 0.5) * radialStep;

    // Cylinder bounds
    auto cylinderBounds =
        std::make_shared<const CylinderBounds>(radius, halfLength);

    // Unique ID for the ring
    uint32_t id = static_cast<uint32_t>(i);

    // Identity transform for the element
    auto elementTransform = std::make_shared<const Transform3>(Transform3::Identity());

    // Create the MtpcDetectorElement
    auto detElem = std::make_shared<MtpcDetectorElement>(
        id,
        elementTransform,
        cylinderBounds,
        radialStep // thickness
    );

    // Store it for later reference
    surfaceStore[id] = detElem;

    // Extract the cylinder surface
    auto cylinderSurface = detElem->surface().getSharedPtr();

    // Create a single-surface SurfaceArray
    // SurfaceArray offers a constructor that takes one Surface
    auto surfaceArray = std::make_unique<SurfaceArray>(cylinderSurface);

    // Create a CylinderLayer
    auto cylinderLayer = CylinderLayer::create(
        Transform3::Identity(),   // transform
        cylinderBounds,           // cylinder bounds
        std::move(surfaceArray),  // single-surface array
        radialStep,               // layer thickness
        nullptr,                  // no approach descriptor
        LayerType::active         // the layer is active
    );

    cylinderLayers.push_back(std::move(cylinderLayer));
  }

  // Create a layer array from the cylinder layers
  LayerArrayCreator::Config layerArrayCreatorConfig;
  LayerArrayCreator layerArrayCreator(layerArrayCreatorConfig);

  auto layerArray = layerArrayCreator.layerArray(
      GeometryContext(),  // geometry context
      cylinderLayers,     // vector of shared_ptr<const Layer>
      innerRadius,        // min radius
      outerRadius,        // max radius
      BinningType::arbitrary,
      BinningValue::binR);

  // Volume bounds (outermost dimensions)
  auto volumeBounds = std::make_shared<CylinderVolumeBounds>(
      innerRadius, outerRadius, halfLength);

  // Assign volume material
  auto volumeMaterial = std::make_shared<HomogeneousVolumeMaterial>(argonGas);

  // Create the tracking volume
  auto trackingVolume = std::make_shared<TrackingVolume>(
      Transform3::Identity(),    // center at origin
      volumeBounds,              // shape/bounds
      volumeMaterial,            // Argon fill
      std::move(layerArray),     // cylinder layers
      nullptr,                   // no contained volumes
      MutableTrackingVolumeVector{},
      "TPCVolume");

  // Finally, create the TrackingGeometry with this single volume as world
  auto trackingGeometry = std::make_unique<TrackingGeometry>(trackingVolume);
  return trackingGeometry;
}

}  // namespace tracking
}  // namespace tdis
