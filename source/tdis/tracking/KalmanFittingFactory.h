#pragma once

#include <JANA/Components/JOmniFactory.h>

#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/MagneticField/ConstantBField.hpp>
#include <Acts/Plugins/Podio/PodioTrackContainer.hpp>
#include <Acts/Plugins/Podio/PodioTrackStateContainer.hpp>
#include <Acts/Propagator/EigenStepper.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/Propagator/SympyStepper.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>
#include <Acts/TrackFitting/KalmanFitter.hpp>
#include <ActsExamples/EventData/Measurement.hpp>
#include <ActsExamples/EventData/Track.hpp>

#include "ActsGeometryService.h"
#include "TrackingTypes.h"
#include "podio_model/DigitizedMtpcMcHitCollection.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/DigitizedMtpcMcTrackCollection.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/Track.h"
#include "podio_model/TrackCollection.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrackSeedCollection.h"
#include "podio_model/TrackerHitCollection.h"
#include "podio_model/Trajectory.h"
#include "podio_model/TrajectoryCollection.h"
#include "services/LogService.hpp"

namespace tdis::tracking {

    // ACTS Types

    using Stepper = Acts::SympyStepper;
    using Propagator = Acts::Propagator<Stepper, Acts::Navigator>;
    using KalmanFitter = Acts::KalmanFitter<Propagator, Acts::VectorMultiTrajectory>;
    using DirectPropagator = Acts::Propagator<Stepper, Acts::DirectNavigator>;
    using DirectKalmanFitter = Acts::KalmanFitter<DirectPropagator, Acts::VectorMultiTrajectory>;

    using TrackContainer = Acts::TrackContainer<Acts::MutablePodioTrackContainer, Acts::MutablePodioTrackStateContainer>;
    using TrackFitterResult = Acts::Result<TrackContainer::TrackProxy>;

    struct KalmanFittingFactory : public JOmniFactory<KalmanFittingFactory> {

        // Data inputs
        PodioInput<tdis::TrackSeed> m_in_trackSeeds{this, {"TrackSeeds"}};

        // Add EDM4eic outputs
        PodioOutput<tdis::Trajectory> m_edm_trajectories{this};
        PodioOutput<tdis::TrackParameters> m_edm_track_params{this};
        PodioOutput<tdis::Track> m_edm_tracks{this};

        Service<ActsGeometryService> m_acts_geo_svc{this};
        Service<services::LogService> m_log_svc{this};

        // Use parameters:
        Parameter<double> m_cfg_bz{this, "bz", 1.5, "Magnetic field in Z (Tesla)"};
        Parameter<std::string> m_acts_level{this, "acts_level", "INFO", "ACTS log level (VERBOSE|DEBUG|INFO|WARNING|ERROR|FATAL)"};



        KalmanFittingFactory();
        void Configure();
        void fillMeasurements(tdis::TrackSeed trackSeed,
                              std::shared_ptr<ActsExamples::MeasurementContainer>& actsMeasurements,
                              std::vector<Acts::SourceLink>& sourceLinks);
        void processTrack(tdis::TrackSeed trackSeed);
        void Execute(int32_t run_number, uint64_t event_number);

    private:
        std::shared_ptr<spdlog::logger> m_log;
        std::shared_ptr<const Acts::Logger> m_acts_logger;

        // using Stepper = Acts::EigenStepper<>;
        // using Propagator = Acts::Propagator<Stepper, Acts::Navigator>;
        // using KF = Acts::KalmanFitter<Propagator, Acts::VectorMultiTrajectory>;

        std::shared_ptr<Propagator> m_propagator;
        //std::shared_ptr<KF> m_kalman_fitter;

        /// Whether to include multiple scattering in the fit
        bool multipleScattering = false;

        /// Whether to include energy loss in the fit
        bool energyLoss = false;

        // Threshold below which we do "reverse filtering"
        // (implemented in the example logic)
        double reverseFilteringMomentumThreshold = 0.;

        // Correction used to handle non-linearities in free->bound transform
        Acts::FreeToBoundCorrection freeToBoundCorrection;



    };

} // namespace tdis::tracking