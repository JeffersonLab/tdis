#pragma once

#include <JANA/Components/JOmniFactory.h>

#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/MagneticField/ConstantBField.hpp>
#include <Acts/Propagator/EigenStepper.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>
#include <Acts/TrackFitting/KalmanFitter.hpp>
#include <ActsExamples/EventData/Track.hpp>

#include "ActsGeometryService.h"
#include "ConfiguredFitter.hpp"
#include "podio_model/DigitizedMtpcMcHitCollection.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/DigitizedMtpcMcTrackCollection.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/Track.h"
#include "podio_model/TrackCollection.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrackerHitCollection.h"
#include "podio_model/Trajectory.h"
#include "podio_model/TrajectoryCollection.h"
#include "services/LogService.hpp"

namespace tdis::tracking {

    class KalmanFittingFactory : public JOmniFactory<KalmanFittingFactory> {
    public:
        // KalmanFittingFactory.hpp
        PodioInput<tdis::DigitizedMtpcMcTrack> m_mc_tracks_input{this, {"DigitizedMtpcMcTrack"}};
        PodioInput<tdis::DigitizedMtpcMcHit> m_mc_hits_input{this, {"DigitizedMtpcMcHit"}};
        PodioInput<edm4eic::TrackerHit> m_tracker_hits_input{this, {"TrackerHit"}};
        PodioInput<edm4eic::Measurement2D> m_measurements_input{this, {"Measurement2D"}};

        // Add EDM4eic outputs
        PodioOutput<edm4eic::Trajectory> m_edm_trajectories{this};
        PodioOutput<edm4eic::TrackParameters> m_edm_track_params{this};
        PodioOutput<edm4eic::Track> m_edm_tracks{this};

        Service<ActsGeometryService> m_acts_geo_svc{this};
        Service<services::LogService> m_log_svc{this};

        // Use parameters:
        Parameter<double> m_bz{this, "bz", 1.5, "Magnetic field in Z (Tesla)"};
        Parameter<std::string> m_csv_out{this, "csv_file", "kf_fit_summary.csv",  "File to append MC-vs-reco track summary"};
        Parameter<std::string> m_acts_level{this, "acts_level", "INFO", "ACTS log level (VERBOSE|DEBUG|INFO|WARNING|ERROR|FATAL)"};

        std::shared_ptr<tdis::ConfiguredFitter> m_fitter ;

        KalmanFittingFactory();
        void Configure();
        void Execute(int32_t run_number, uint64_t event_number);

    private:
        std::shared_ptr<spdlog::logger> m_logger;
        std::shared_ptr<const Acts::Logger> m_acts_logger;

        using Stepper = Acts::EigenStepper<>;
        using Propagator = Acts::Propagator<Stepper, Acts::Navigator>;
        using KF = Acts::KalmanFitter<Propagator, Acts::VectorMultiTrajectory>;

        std::shared_ptr<Propagator> m_propagator;
        std::shared_ptr<KF> m_kalman_fitter;
        std::ofstream m_csv;
    };

} // namespace tdis::tracking