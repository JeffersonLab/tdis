#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

// Needed so that the dynamic_cast to CylinderBounds sees the full definition:
#include <Acts/Surfaces/CylinderBounds.hpp>
#include <Acts/Surfaces/Surface.hpp>

#include "PadGeometryHelper.hpp"
#include "podio_model/DigitizedMtpcMcHit.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/MutableTrackerHit.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/TrackerHitCollection.h"

namespace {
    inline double get_resolution(const double pixel_size) {
        // 1 / sqrt(12) to approximate uniform distribution RMS
        constexpr const double sqrt_12 = 3.4641016151;
        return pixel_size / sqrt_12;
    }
    inline double get_variance(const double pixel_size) {
        const double res = get_resolution(pixel_size);
        return res * res;
    }
}  // namespace

namespace tdis::tracking {

    struct ReconstructedHitFactory : public JOmniFactory<ReconstructedHitFactory> {
        // Inputs and outputs
        PodioInput<tdis::DigitizedMtpcMcHit> m_mc_hits_in{this, {"DigitizedMtpcMcHit"}};
        PodioOutput<edm4eic::TrackerHit>     m_tracker_hits_out{this, "TrackerHit"};
        PodioOutput<edm4eic::Measurement2D>  m_measurements_out{this, "Measurement2D"};

        Service<ActsGeometryService> m_service_geometry{this};
        Service<services::LogService> m_service_log{this};

        Parameter<bool> m_cfg_use_true_pos{
            this,
            "acts:use_true_position",
            false,
            "Use true hits xyz instead of digitized one"
        };

        std::shared_ptr<spdlog::logger> m_log;

        void Configure() {
            // Ensure geometry is available, then set up logging
            m_service_geometry();
            m_log = m_service_log->logger("tracking:hit_reco");

        }

        void ChangeRun(int32_t /*run_nr*/) {
            // No-op for this example
        }

        void Execute(int32_t /*run_nr*/, uint64_t event_index) {
            using namespace Acts::UnitLiterals;  // For e.g. 1_cm, 1_um, etc.

            auto rec_hits     = std::make_unique<edm4eic::TrackerHitCollection>();
            auto measurements = std::make_unique<edm4eic::Measurement2DCollection>();

            // Retrieve plane Z-positions from geometry
            auto plane_positions = m_service_geometry->GetPlanePositions();

            m_log->trace("ReconstructedHitFactory, reconstructing event: {}", event_index);

            for (auto mc_hit : *m_mc_hits_in()) {

                // Basic geometry indices
                const int plane = mc_hit.getPlane();
                const int ring  = mc_hit.getRing();
                const int pad   = mc_hit.getPad();
                const double z_to_gem = mc_hit.getZToGem();

                if (pad == -999) break;
                // Convert ring+pad to (x,y)
                auto [pad_x, pad_y] = getPadCenter(ring, pad);
                double plane_z = plane_positions[plane];
                double ring_radius = getRingCenterRadius(ring);

                // Adjust sign for “even vs odd plane”
                double calc_z = plane_z + (plane % 2 ? -z_to_gem : z_to_gem);

                double pad_center_r = std::sqrt(pad_x * pad_x + pad_y * pad_y);

                m_log->trace(
                    "Plane {}, ring {}, pad {}, ring_r {:.2f} pad_r {:.2f} "
                    "pad_x {:.2f} true_x {:.2f} pad_y {:.2f} true_y {:.2f} "
                    "plane_z {:.2f} z_to_gem {:.2f} calc_z {:.2f} true_z {:.2f}",
                    plane, ring, pad, ring_radius, pad_center_r,
                    pad_x, mc_hit.getTruePosition().x,
                    pad_y, mc_hit.getTruePosition().y,
                    plane_z, z_to_gem, calc_z, mc_hit.getTruePosition().z);

                // Choose position: either true or digitized
                edm4hep::Vector3f position;
                if (m_cfg_use_true_pos() && !std::isnan(mc_hit.getTruePosition().x)) {
                    position.x = mc_hit.getTruePosition().x;
                    position.y = mc_hit.getTruePosition().y;
                    position.z = mc_hit.getTruePosition().z;
                } else {
                    position.x = (float) pad_x;
                    position.y = (float) pad_y;
                    position.z = (float) calc_z;
                }

                // Covariance estimate
                double max_dimension = std::max(getPadApproxWidth(ring), getPadHight());
                double xy_variance   = get_variance(max_dimension);

                // For now, put some placeholder 1 cm^2 in z
                edm4eic::CovDiag3f cov{static_cast<float>(xy_variance),
                                       static_cast<float>(xy_variance),
                                       static_cast<float>(1_cm)};

                uint32_t cell_id = 1'000'000 * mc_hit.getPlane() + 1'000 * mc_hit.getRing() + mc_hit.getPad();

                auto hit = rec_hits->create(
                    cell_id,
                    position,
                    cov,
                    static_cast<float>(mc_hit.getTime()),
                    static_cast<float>(1_ns),   // Time resolution (placeholder)
                    static_cast<float>(mc_hit.getAdc()),
                    0.0F
                );
                hit.setRawHit(mc_hit);

                // Retrieve the geometry element (Acts cylinder) for this ring
                auto acts_det_element = m_service_geometry().GetDetectorCylinder(mc_hit.getRing());
                auto& surfaceRef      = acts_det_element->surface();
                auto geometryId = surfaceRef.geometryId().value();

                // ----------------------------------------------------------------------
                // Print or log the surface type
                // In ACTS v37.4.0, surface->type() returns Acts::RegularSurface::SurfaceType
                // ----------------------------------------------------------------------
                Acts::RegularSurface::SurfaceType sType = surfaceRef.type();

                // Convert enumerator to string
                std::string sTypeStr = "UnknownSurfaceType";

                // Since the largest enumerator is 'Other' = 7, let's check
                size_t idx = static_cast<size_t>(sType);
                if (idx < static_cast<size_t>(Acts::RegularSurface::SurfaceType::Other)) {
                    switch (sType) {
                    case Acts::RegularSurface::SurfaceType::Cone:
                        sTypeStr = "Cone";
                        break;
                    case Acts::RegularSurface::SurfaceType::Cylinder:
                        sTypeStr = "Cylinder";
                        break;
                    case Acts::RegularSurface::SurfaceType::Disc:
                        sTypeStr = "Disc";
                        break;
                    case Acts::RegularSurface::SurfaceType::Perigee:
                        sTypeStr = "Perigee";
                        break;
                    case Acts::RegularSurface::SurfaceType::Plane:
                        sTypeStr = "Plane";
                        break;
                    case Acts::RegularSurface::SurfaceType::Straw:
                        sTypeStr = "Straw";
                        break;
                    case Acts::RegularSurface::SurfaceType::Curvilinear:
                        sTypeStr = "Curvilinear";
                        break;
                    default:
                        sTypeStr = "Other";
                        break;
                    }
                }

                m_log->trace("Surface type for ring {} = {}", ring, sTypeStr);

                // If this surface is a cylinder, log cylinder parameters
                if (sType == Acts::RegularSurface::SurfaceType::Cylinder) {
                    auto& surfBounds = surfaceRef.bounds();

                    // Downcast to CylinderBounds (make sure we've included
                    // <Acts/Surfaces/CylinderBounds.hpp>)
                    if (const auto* cylBounds
                        = dynamic_cast<const Acts::CylinderBounds*>(&surfBounds)) {
                        double radius     = cylBounds->get(Acts::CylinderBounds::eR);
                        double halfLength = cylBounds->get(Acts::CylinderBounds::eHalfLengthZ);
                        m_log->trace("  Cylinder radius     = {} mm", radius);
                        m_log->trace("  Cylinder halfLength = {} mm", halfLength);
                    } else {
                        m_log->trace("  [Error] Bounds are not CylinderBounds!");
                    }
                }

                // ----------------------------------------------------------------------
                // Attempt to find local 2D measurement coordinates on the surface
                // ----------------------------------------------------------------------
                const auto& hit_pos = hit.getPosition();  // 3D position from above
                Acts::Vector2 loc   = Acts::Vector2::Zero();

                // Acts tolerance for checking if a point is close to surface
                auto onSurfaceTolerance = 1 * Acts::UnitConstants::mm;

                try {
                    // Convert global position (x,y,plane_z) to local coords
                    // geometry context is empty by default
                    Acts::Vector2 pos = surfaceRef
                        .globalToLocal(
                            Acts::GeometryContext(),
                            {hit_pos.x, hit_pos.y, plane_z},
                            Acts::Vector3::Zero(),
                            onSurfaceTolerance
                        )
                        .value();

                    // Fill in loc
                    loc[Acts::eBoundLoc0] = pos[0];
                    loc[Acts::eBoundLoc1] = pos[1];

                } catch (std::exception& ex) {
                    auto message = fmt::format("Can't convert globalToLocal for hit: plane={} ring={} pad={} "
                        "RecoHit x={} y={} z={}. Reason: {}",
                        mc_hit.getPlane(), mc_hit.getRing(), mc_hit.getPad(),
                        hit_pos.x, hit_pos.y, hit_pos.z, ex.what());
                    m_log->warn(message);
                    continue;
                }

                // If needed, we can log the center of the surface in global coords
                if (m_log->level() <= spdlog::level::trace) {
                    auto surf_center = surfaceRef.center(Acts::GeometryContext());
                    m_log->trace(
                        "   Hit position     : {:>10.2f} {:>10.2f} {:>10.2f}",
                        hit_pos.x, hit_pos.y, hit_pos.z
                    );
                    m_log->trace(
                        "   Surface center   : {:>10.2f} {:>10.2f} {:>10.2f}",
                        surf_center.x(), surf_center.y(), surf_center.z()
                    );
                    m_log->trace(
                        "   Local coords     : {:>10.2f} {:>10.2f}",
                        loc[Acts::eBoundLoc0], loc[Acts::eBoundLoc1]
                    );
                }

                // Create a new measurement2D
                auto meas2D = measurements->create();

                meas2D.setSurface(surfaceRef.geometryId().value());
                meas2D.setLoc({static_cast<float>(loc[0]), static_cast<float>(loc[1])});
                meas2D.setTime(hit.getTime());

                // Covariance on local coords (no off-diagonal for now), plus time
                meas2D.setCovariance({
                    cov(0, 0),
                    cov(1, 1),
                    hit.getTimeError() * hit.getTimeError(),
                    cov(0, 1)
                });
                meas2D.addToWeights(1.0); // Example usage
                meas2D.addToHits(hit);
            }

            // Write out the results
            m_tracker_hits_out()   = std::move(rec_hits);
            m_measurements_out()   = std::move(measurements);
        }
    };
}  // namespace tdis::tracking
