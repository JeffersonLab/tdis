#pragma once

#include <JANA/JFactory.h>
#include <JANA/Components/JOmniFactory.h>

namespace tdis::tracking {



    struct MyClusterFactory : public JOmniFactory<MyClusterFactory> {

        PodioInput<ExampleCluster> m_protoclusters_in {this};
        PodioOutput<ExampleCluster> m_clusters_out {this};


        void Configure() {
        }

        void ChangeRun(int32_t /*run_nr*/) {
        }

        void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {

            auto cs = std::make_unique<ExampleClusterCollection>();

            for (auto protocluster : *m_protoclusters_in()) {
                auto cluster = MutableExampleCluster(protocluster.energy() + 1000);
                cluster.addClusters(protocluster);
                cs->push_back(cluster);
            }

            m_clusters_out() = std::move(cs);
        }
    };

}