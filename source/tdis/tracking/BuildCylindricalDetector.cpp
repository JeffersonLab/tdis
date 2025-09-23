// This file is part of the Acts project.
//
// Copyright (C) 2020-2021 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "BuildCylindricalDetector.h"

#include <spdlog/spdlog.h>  // Ensure SPDLog header is included

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
#include <unordered_map>
#include <vector>

#include "MtpcDetectorElement.hpp"

/**
 * Build a cylindrical geometry for the mTPC. One cylinder layer per ring,
 * each covering the full 55 cm in z. The ring's radius is used for
 * CylinderBounds, with a 'thickness' in radius = radialStep.
 *
 * @param log             The spdlog logger
 * @param gctx            The geometry context (unused here, but passed in for completeness)
 * @param surfaceStore    A map to store created MtpcDetectorElement references
 * @return                The resulting Acts::TrackingGeometry
 */
std::unique_ptr<const Acts::TrackingGeometry> tdis::tracking::buildCylindricalDetector(
    const std::shared_ptr<spdlog::logger>& log,
    const MtpcDetectorElement::ContextType& gctx,
    std::unordered_map<uint32_t, std::shared_ptr<MtpcDetectorElement>>& surfaceStore)
{
    using namespace Acts;
    using namespace Acts::UnitLiterals;
    namespace ActsUnits = Acts::UnitConstants;

    //
    // -- Start with a general initialization message
    //
    // If your environment uses a custom init(...) method, adapt as necessary:
    log->info("Initializing buildCylindricalDetector() at log level '{}'",
              spdlog::level::to_string_view(log->level()));

    // Define Argon gas material properties at STP
    float radiationLength   = 19.55 * ActsUnits::m;    // ~19.55 m in mm
    float interactionLength = 70.0 * ActsUnits::m;     // ~70.0 m in mm
    float atomicMass        = 39.948;              // Argon
    float atomicNumber      = 18;                  // Argon
    float massDensity       = 1.66e-6 * ActsUnits::g / 1 * ActsUnits::mm3; // g/mm^3

    log->debug("Constructing ArgonGas Material: RL={} mm, IL={} mm, A={}, Z={}, density={} g/mm^3",
               radiationLength, interactionLength, atomicMass, atomicNumber, massDensity);

    // Create Argon gas material
    Material argonGas = Material::fromMassDensity(radiationLength, interactionLength, atomicMass, atomicNumber, massDensity);

    // --------------------
    // Geometry parameters
    // --------------------
    double innerRadius     =  5 * ActsUnits::cm;   //  5 cm
    double outerRadius     = 15 * ActsUnits::cm;   // 15 cm
    double cylinderLength  = 55 * ActsUnits::cm;   // 55 cm total
    double halfLength      = cylinderLength / 2.0;
    int    numRings        = 21;       // 21 concentric rings
    double radialStep      = (outerRadius - innerRadius) / numRings;

    log->info("Building cylindrical mTPC geometry with:");
    log->info("  innerRadius = {} mm, outerRadius = {} mm, cylinderLength = {} mm",
        innerRadius/ActsUnits::mm, outerRadius/ActsUnits::mm, cylinderLength/ActsUnits::mm);
    log->info("  halfLength  = {} mm, numRings = {}, radialStep = {} mm",
        halfLength/ActsUnits::mm, numRings/ActsUnits::mm, radialStep/ActsUnits::mm);

    // We store the final layers as generic Layer pointers
    std::vector<std::shared_ptr<const Layer>> cylinderLayers;
    cylinderLayers.reserve(numRings);

    //
    // Build each ring as one CylinderLayer
    //
    for (int i = 0; i < numRings; ++i) {
        // Calculate the radius of the current ring
        double ringRadius = innerRadius + (i + 0.5) * radialStep;

        // Cylinder bounds
        auto cylinderBounds = std::make_shared<const CylinderBounds>(ringRadius, halfLength);

        // Unique ID for the ring
        uint32_t id = static_cast<uint32_t>(i);

        log->info("  Ring id={} => radius = {:.3f} mm, halfLength = {:.3f} mm, thickness = {:.3f}, radius+step={:.3f}",
                   id, ringRadius, halfLength, radialStep, ringRadius+radialStep);

        // Identity transform for the element
        auto elementTransform = std::make_shared<const Transform3>(Transform3::Identity());

        // Create the MtpcDetectorElement
        auto detElem = std::make_shared<MtpcDetectorElement>(id,
                                                             elementTransform,
                                                             cylinderBounds,
                                                             radialStep);
        // Store it for later reference
        surfaceStore[id] = detElem;

        // Extract the cylinder surface from the element
        auto cylinderSurface = detElem->surface().getSharedPtr();

        auto setGeoId = cylinderSurface->geometryId();
        std::cout<<setGeoId<<std::endl;

        // Create a single-surface SurfaceArray
        // For multiple surfaces, you'd push them into a vector,
        // but here we only have one per ring
        auto surfaceArray = std::make_unique<SurfaceArray>(cylinderSurface);

        // Create a CylinderLayer
        auto cylinderLayer = CylinderLayer::create(
            Transform3::Identity(),     // transform
            cylinderBounds,             // cylinder bounds
            std::move(surfaceArray),    // single-surface array
            radialStep,                 // layer thickness
            nullptr,                    // no approach descriptor
            LayerType::active           // the layer is active
        );


        cylinderLayers.push_back(std::move(cylinderLayer));
    }

    // Create a layer array from the cylinder layers
    LayerArrayCreator::Config layerArrayCreatorConfig;
    LayerArrayCreator layerArrayCreator(layerArrayCreatorConfig);

    log->debug("Creating LayerArray with BinningType::arbitrary in R.");
    auto layerArray = layerArrayCreator.layerArray(
        GeometryContext(),    // geometry context
        cylinderLayers,       // vector of shared_ptr<const Layer>
        innerRadius,          // min radius
        outerRadius,          // max radius
        BinningType::equidistant,       // (!) That is super important if end of one volume is beginning of the next
        AxisDirection::AxisR
    );

    //
    // Volume bounds (outermost dimensions)
    //
    auto volumeBounds = std::make_shared<CylinderVolumeBounds>(innerRadius, outerRadius, halfLength);
    log->info("Volume bounds: rIn={} mm, rOut={} mm, halfZ={} mm",
              innerRadius, outerRadius, halfLength);

    // Assign volume material
    auto volumeMaterial = std::make_shared<HomogeneousVolumeMaterial>(argonGas);

    // Create the tracking volume
    log->debug("Creating TrackingVolume 'TPCVolume'...");
    auto trackingVolume = std::make_shared<TrackingVolume>(
        Transform3::Identity(),          // center at origin
        volumeBounds,                    // shape/bounds
        volumeMaterial,                  // Argon fill
        std::move(layerArray),           // cylinder layers
        nullptr,                         // no contained volumes
        MutableTrackingVolumeVector{},
        "TPCVolume"
    );


    // Finally, create the TrackingGeometry with this single volume as the world
    auto trackingGeometry = std::make_unique<TrackingGeometry>(trackingVolume);


    // (!) This is not only debugging output, but also is a test that:
    // Surfaces we think we have, are the same as surfaces in tracking geometry.
    log->info("Surface IDs in detector elements:");
    for (const auto& pair : surfaceStore) {
        auto ringId = pair.first;
        auto surfaceGeoId = pair.second->surface().geometryId();
        auto surfaceFromTrkGeo = trackingGeometry->findSurface(surfaceGeoId);
        if (!surfaceFromTrkGeo) {
            log->critical("For ring = {}, we can't find back the surface with id = {}. It is trackingGeometry->findSurface==NULL. Track fitting will fail soon (!)", ringId, surfaceGeoId.volume());
            continue;
        }

        log->info("   ring: {} => Surface ID: {}", ringId, surfaceFromTrkGeo->geometryId().value());
    }
    log->info("Done building cylindrical mTPC geometry. Returning TrackingGeometry.");
    return trackingGeometry;
}
