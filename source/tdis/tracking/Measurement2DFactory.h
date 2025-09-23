#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

#include "PadGeometryHelper.hpp"

#include "podio_model/MutableTrackerHit.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/TrackerHitCollection.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/Measurement2DCollection.h"


namespace tdis::tracking {

    struct Measurement2DFactory : public JOmniFactory<Measurement2DFactory> {
        PodioInput<tdis::TrackerHit> m_mc_hits_in {this, {"TrackerHit"}};
        PodioOutput<tdis::Measurement2D> m_tracker_hits_out {this, "Measurement2D"};
        Service<ActsGeometryService> m_service_geometry{this};
        Parameter<bool> m_cfg_use_true_pos{this, "acts:use_true_position", true,"Use true hits xyz instead of digitized one"};

        void Configure() {
            m_service_geometry();
        }

        void ChangeRun(int32_t /*run_nr*/) {
        }

        void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {
            using namespace Acts::UnitLiterals;


            auto measurements = std::make_unique<tdis::Measurement2DCollection>();
            auto plane_positions = m_service_geometry->GetPlanePositions();


            m_tracker_hits_out() = std::move(measurements);
        }
    };
}