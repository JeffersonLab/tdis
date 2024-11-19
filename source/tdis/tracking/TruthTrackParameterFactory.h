#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

#include "PadGeometryHelper.hpp"
#include "podio_model/DigitizedMtpcMcHit.h"
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
} // namespace

namespace tdis::tracking {

    struct TruthTrackParameterFactory : public JOmniFactory<TruthTrackParameterFactory> {
        PodioInput<tdis::DigitizedMtpcMcHit> m_mc_hits_in {this, {"DigitizedMtpcMcHit"}};
        PodioOutput<edm4eic::TrackerHit> m_tracker_hits_out {this, "TrackerHit"};
        Service<ActsGeometryService> m_service_geometry{this};
        Parameter<bool> m_cfg_use_true_pos{this, "acts:use_true_position", true,"Use true hits xyz instead of digitized one"};

        void Configure() {
            m_service_geometry();
        }

        void ChangeRun(int32_t /*run_nr*/) {
        }

        void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {
            using namespace Acts::UnitLiterals;


            auto rec_hits = std::make_unique<edm4eic::TrackerHitCollection>();
            auto plane_positions = m_service_geometry->GetPlanePositions();

            for (auto mc_hit : *m_mc_hits_in()) {

                const int plane = mc_hit.plane();
                const int ring = mc_hit.ring();
                const int pad = mc_hit.pad();
                const double z_to_gem = mc_hit.zToGem();

                auto [pad_x,pad_y] = getPadCenter(ring, pad);
                double plane_z = m_service_geometry().GetPlanePositions()[plane];


                // Planes are set back to back like this:
                // |     ||     ||     ||    |
                // This means that zToPlane is positive for even planes and negative for odd
                // i.e.
                // z = plane_z + zToPlane // for even
                // z = plane_z - zToPlane // for odd
                double calc_z = plane_z + (plane % 2 ? -z_to_gem : z_to_gem);


                // Position
                edm4hep::Vector3f position;
                if(m_cfg_use_true_pos() && !std::isnan(mc_hit.truePosition().x)) {
                    position.x = mc_hit.truePosition().x;
                    position.y = mc_hit.truePosition().y;
                    position.z = mc_hit.truePosition().z;
                }
                else {
                    position.x = pad_x;
                    position.y = pad_y;
                    position.z = calc_z;
                }

                // Covariance
                double max_dimension = std::max(getPadApproxWidth(ring), getPadHight());
                double xy_variance = get_variance(max_dimension);
                edm4eic::CovDiag3f cov{xy_variance, xy_variance, 1_cm};  // TODO correct Z variance

                uint32_t cell_id = 1000000*mc_hit.plane() + 1000*mc_hit.ring() + mc_hit.pad();

                rec_hits->create(
                    cell_id,
                    position,
                    cov,
                    static_cast<float>(mc_hit.time()),
                    static_cast<float>(1_ns),                   // TODO correct time resolution
                    static_cast<float>(mc_hit.adc()),
                    0.0F);
            }
            m_tracker_hits_out() = std::move(rec_hits);
        }
    };
}