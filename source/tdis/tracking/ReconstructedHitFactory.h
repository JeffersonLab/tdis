#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

#include "PadGeometryHelper.hpp"
#include "podio_model/DigitizedMtpcMcHit.h"
#include "podio_model/MutableTrackerHit.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/TrackerHitCollection.h"

namespace tdis::tracking {



    struct ReconstructedHitFactory : public JOmniFactory<ReconstructedHitFactory> {

        PodioInput<tdis::DigitizedMtpcMcHit> m_mc_hits_in {this};
        PodioOutput<edm4eic::TrackerHit> m_tracker_hits_out {this};
        Service<ActsGeometryService> m_service_geometry{this};


        void Configure() {
            m_service_geometry();

        }

        void ChangeRun(int32_t /*run_nr*/) {
        }

        void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {

            auto result_hits = std::make_unique<edm4eic::TrackerHitCollection>();

            for (auto mc_hit : *m_mc_hits_in()) {

                auto [x,y] = getPadCenter(mc_hit.ring(), mc_hit.pad());
                //#double z = getRing

                //std::uint64_t cellID, edm4hep::Vector3f position, edm4eic::CovDiag3f positionError, float time, float timeError, float edep, float edepError
                //cluster.addClusters(protocluster);
                //cs->push_back(cluster);
            }

            m_tracker_hits_out() = std::move(result_hits);

            /*
             *auto rec_hits { std::make_unique<edm4eic::TrackerHitCollection>() };

    for (const auto& raw_hit : raw_hits) {

        auto id = raw_hit.getCellID();

        // Get position and dimension
        auto pos = m_converter->position(id);
        auto dim = m_converter->cellDimensions(id);

        // >oO trace
        if(m_log->level() == spdlog::level::trace) {
            m_log->trace("position x={:.2f} y={:.2f} z={:.2f} [mm]: ", pos.x()/ mm, pos.y()/ mm, pos.z()/ mm);
            m_log->trace("dimension size: {}", dim.size());
            for (size_t j = 0; j < std::size(dim); ++j) {
                m_log->trace(" - dimension {:<5} size: {:.2}",  j, dim[j]);
            }
        }

        // Note about variance:
        //    The variance is used to obtain a diagonal covariance matrix.
        //    Note that the covariance matrix is written in DD4hep surface coordinates,
        //    *NOT* global position coordinates. This implies that:
        //      - XY segmentation: xx -> sigma_x, yy-> sigma_y, zz -> 0, tt -> 0
        //      - XZ segmentation: xx -> sigma_x, yy-> sigma_z, zz -> 0, tt -> 0
        //      - XYZ segmentation: xx -> sigma_x, yy-> sigma_y, zz -> sigma_z, tt -> 0
        //    This is properly in line with how we get the local coordinates for the hit
        //    in the TrackerSourceLinker.
 #if EDM4EIC_VERSION_MAJOR >= 7
       auto rec_hit =
 #endif
       rec_hits->create(
            raw_hit.getCellID(), // Raw DD4hep cell ID
            edm4hep::Vector3f{static_cast<float>(pos.x() / mm), static_cast<float>(pos.y() / mm), static_cast<float>(pos.z() / mm)}, // mm
            edm4eic::CovDiag3f{get_variance(dim[0] / mm), get_variance(dim[1] / mm), // variance (see note above)
            std::size(dim) > 2 ? get_variance(dim[2] / mm) : 0.},
                static_cast<float>((double)(raw_hit.getTimeStamp()) / 1000.0), // ns
            m_cfg.timeResolution,                            // in ns
            static_cast<float>(raw_hit.getCharge() / 1.0e6),   // Collected energy (GeV)
            0.0F);                                       // Error on the energy
#if EDM4EIC_VERSION_MAJOR >= 7
        rec_hit.setRawHit(raw_hit);
#endif
             * */
        }

        private:
            static inline double get_resolution(const double pixel_size) {
              constexpr const double sqrt_12 = 3.4641016151;
              return pixel_size / sqrt_12;
          }

          static inline double get_variance(const double pixel_size) {
                const double res = get_resolution(pixel_size);
                return res * res;
            }

    };

}