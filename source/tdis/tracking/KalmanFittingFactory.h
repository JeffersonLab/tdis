#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

#include "services/LogService.hpp"
#include "ActsGeometryService.h"

// PODIO input:
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/Measurement2D.h"

// PODIO output:
// We will produce an ActsExamples::ConstTrackContainer (Acts vector tracks)
#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <ActsExamples/EventData/Track.hpp> // for ActsExamples::ConstTrackContainer type

namespace tdis {
        namespace tracking {

                /// JANA2 factory which performs a simple Kalman fit using:
                ///  - MC truth parameters as initial guesses,
                ///  - 2D measurements for the hits,
                ///  - Gains matrix-based Kalman filter in Acts.
                struct KalmanFittingFactory : public JOmniFactory<KalmanFittingFactory> {

                        /// Inputs (from your PODIO data model)
                        PodioInput<tdis::DigitizedMtpcMcTrack> m_mc_tracks_in {
                                this,
                                {"DigitizedMtpcMcTrack"}
                        };

                        /// We will read the 2D measurements from your ReconstructedHitFactory
                        PodioInput<edm4eic::Measurement2D> m_measurements_in {
                                this,
                                {"Measurement2D"}
                        };

                        /// Output: We produce a final Acts track container
                        Output<ActsExamples::ConstTrackContainer> m_acts_tracks_out {
                                this,
                                "ConstTrackContainer"
                            };

                        /// Services
                        Service<services::LogService>        m_service_log {this};
                        Service<tdis::tracking::ActsGeometryService> m_service_geo {this};

                        /// JANA2 callbacks
                        void Configure();
                        void ChangeRun(int32_t runNumber) override;
                        void Execute(int32_t runNumber, uint64_t eventNumber);

                private:
                        std::shared_ptr<spdlog::logger>      m_log;
                };

        } // end namespace tracking
} // end namespace tdis
