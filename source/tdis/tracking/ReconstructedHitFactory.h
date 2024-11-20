#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

#include "PadGeometryHelper.hpp"
#include "podio_model/DigitizedMtpcMcHit.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/MutableTrackerHit.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/TrackerHitCollection.h"

namespace {
    inline double get_resolution(const double pixel_size) {
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
        PodioInput<tdis::DigitizedMtpcMcHit> m_mc_hits_in{this, {"DigitizedMtpcMcHit"}};
        PodioOutput<edm4eic::TrackerHit> m_tracker_hits_out{this, "TrackerHit"};
        PodioOutput<edm4eic::Measurement2D> m_measurements_out{this, "Measurement2D"};
        Service<ActsGeometryService> m_service_geometry{this};
        Service<services::LogService> m_service_log{this};
        Parameter<bool> m_cfg_use_true_pos{this, "acts:use_true_position", true, "Use true hits xyz instead of digitized one"};

        std::shared_ptr<spdlog::logger> m_log;

        void Configure() {
            m_service_geometry();
            m_log = m_service_log->logger("tracking:hit_reco");
        }

        void ChangeRun(int32_t /*run_nr*/) {}

        void Execute(int32_t /*run_nr*/, uint64_t event_index) {
            using namespace Acts::UnitLiterals;

            auto rec_hits = std::make_unique<edm4eic::TrackerHitCollection>();
            auto measurements = std::make_unique<edm4eic::Measurement2DCollection>();
            auto plane_positions = m_service_geometry->GetPlanePositions();

            m_log->trace("ReconstructedHitFactory, reconstructing event: {}", event_index);

            for (auto mc_hit : *m_mc_hits_in()) {
                const int plane = mc_hit.plane();
                const int ring = mc_hit.ring();
                const int pad = mc_hit.pad();
                const double z_to_gem = mc_hit.zToGem();

                m_log->trace("Plane {}, ring {}, pad {}, z_to_gem {}. True x {}, y {}, z {}", plane, ring, pad, z_to_gem,
                    mc_hit.truePosition().x, mc_hit.truePosition().y, mc_hit.truePosition().z);


                auto [pad_x, pad_y] = getPadCenter(ring, pad);
                double plane_z = plane_positions[plane];

                // Planes are set back to back like this:
                // |     ||     ||     ||    |
                // This means that zToPlane is positive for even planes and negative for odd
                // i.e.
                // z = plane_z + zToPlane // for even
                // z = plane_z - zToPlane // for odd
                double calc_z = plane_z + (plane % 2 ? -z_to_gem : z_to_gem);

                // Position
                edm4hep::Vector3f position;
                if (m_cfg_use_true_pos() && !std::isnan(mc_hit.truePosition().x)) {
                    position.x = mc_hit.truePosition().x;
                    position.y = mc_hit.truePosition().y;
                    position.z = mc_hit.truePosition().z;
                } else {
                    position.x = pad_x;
                    position.y = pad_y;
                    position.z = calc_z;
                }

                // Covariance
                double max_dimension = std::max(getPadApproxWidth(ring), getPadHight());
                double xy_variance = get_variance(max_dimension);
                edm4eic::CovDiag3f cov{xy_variance, xy_variance, 1_cm};  // TODO correct Z variance

                uint32_t cell_id = 1000000 * mc_hit.plane() + 1000 * mc_hit.ring() + mc_hit.pad();

                auto hit
                    = rec_hits->create(cell_id, position, cov, static_cast<float>(mc_hit.time()),
                                       static_cast<float>(1_ns),  // TODO correct time resolution
                                       static_cast<float>(mc_hit.adc()), 0.0F);

                hit.rawHit(mc_hit);

                auto acts_det_element = m_service_geometry().GetDetectorElement(mc_hit.plane());

                // Measurement 2d

                // variable surf_center not used anywhere;

                const auto& hit_pos = hit.position();  // 3d position

                Acts::Vector2 loc = Acts::Vector2::Zero();
                Acts::Vector2 pos;
                auto onSurfaceTolerance
                    = 0.1 * Acts::UnitConstants::um;  // By default, ACTS uses 0.1 micron as the on
                                                      // surface tolerance

                try {
                    // transform global position into local coordinates
                    // geometry context contains nothing here
                    pos = acts_det_element->surface()
                              .globalToLocal(Acts::GeometryContext(),
                                             {hit_pos.x, hit_pos.y, plane_z},
                                             {0, 0, 0},
                                             onSurfaceTolerance)
                              .value();

                    loc[Acts::eBoundLoc0] = pos[0];
                    loc[Acts::eBoundLoc1] = pos[1];
                } catch (std::exception& ex) {
                    m_log->warn(
                        "Can't convert globalToLocal for hit: plane={} ring={} pad={} RecoHit x={} y={} z={}. Reason: {}",
                        mc_hit.plane(), mc_hit.ring(), mc_hit.pad(), hit_pos.x, hit_pos.y, hit_pos.z, ex.what());
                    continue;
                }

                if (m_log->level() <= spdlog::level::trace) {
                    double surf_center_x = acts_det_element->surface().center(Acts::GeometryContext()).transpose()[0];
                    double surf_center_y = acts_det_element->surface().center(Acts::GeometryContext()).transpose()[1];
                    double surf_center_z = acts_det_element->surface().center(Acts::GeometryContext()).transpose()[2];
                    m_log->trace("   hit position     : {:>10.2f} {:>10.2f} {:>10.2f}", hit_pos.x, hit_pos.y, hit_pos.z);
                    m_log->trace("   surface center   : {:>10.2f} {:>10.2f} {:>10.2f}", surf_center_x, surf_center_y, surf_center_z);
                    m_log->trace("   acts local center: {:>10.2f} {:>10.2f}", pos.transpose()[0], pos.transpose()[1]);
                    m_log->trace("   acts loc pos     : {:>10.2f} {:>10.2f}", loc[Acts::eBoundLoc0],
                                 loc[Acts::eBoundLoc1]);
                }

                auto meas2D = measurements->create();
                meas2D.surface(acts_det_element->surface().geometryId().value());  // Surface for bound coordinates (geometryID)
                meas2D.loc({static_cast<float>(pos[0]), static_cast<float>(pos[1])});  // 2D location on surface
                meas2D.time(hit.time());                   // Measurement time
                // fixme: no off-diagonal terms. cov(0,1) = cov(1,0)??
                meas2D.covariance({cov(0, 0), cov(1, 1), hit.timeError() * hit.timeError(), cov(0, 1)});  // Covariance on location and time
                meas2D.addweights(1.0);          // Weight for each of the hits, mirrors hits array
                meas2D.addhits(hit);
            }
            m_tracker_hits_out() = std::move(rec_hits);
            m_measurements_out() = std::move(measurements);
        }
    };
}  // namespace tdis::tracking