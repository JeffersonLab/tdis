#pragma once

#include <JANA/JFactory.h>
#include <JANA/Components/JOmniFactory.h>

namespace tdis::tracking {



    struct KalmanFittingFactory : public JOmniFactory<KalmanFittingFactory> {

        void Configure() {
        }

        void ChangeRun(int32_t /*run_nr*/) {
        }

        void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {


        }
    };

}