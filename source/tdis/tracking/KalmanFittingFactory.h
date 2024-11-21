#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>
#include <spdlog/logger.h>

#include <ActsExamples/EventData/Track.hpp>

#include "ActsGeometryService.h"
#include "podio_model/DigitizedMtpcMcHit.h"
#include "podio_model/Measurement2D.h"
#include "podio_model/TrackParameters.h"
#include "services/LogService.hpp"

namespace tdis::tracking {

    struct KalmanFittingFactory : public JOmniFactory<KalmanFittingFactory> {
        PodioInput<edm4eic::TrackParameters> m_parameters_input {this};
        PodioInput<edm4eic::Measurement2D> m_measurements_input {this};
//        // Output<ActsExamples::Trajectories> m_acts_trajectories_output {this};
        Output<ActsExamples::ConstTrackContainer> m_acts_tracks_output {this, "ConstTrackContainer"};
//
//        Parameter<std::vector<double>> m_etaBins {this, "EtaBins", {}, "Eta Bins for ACTS CKF tracking reco"};
//        Parameter<std::vector<double>> m_chi2CutOff {this, "Chi2CutOff", {15.}, "Chi2 Cut Off for ACTS CKF tracking"};
//        Parameter<std::vector<size_t>> m_numMeasurementsCutOff {this, "NumMeasurementsCutOff", {10}, "Number of measurements Cut Off for ACTS CKF tracking"};
//
//
        Service<ActsGeometryService> m_serviceGeometry{this};
        Service<services::LogService> m_service_log{this};
//        Parameter<bool> m_cfg_use_true_pos{this, "acts:use_true_position", true, "Use true hits xyz instead of digitized one"};
//
//
//        // Construct a propagator using a constant magnetic field along z.
        template <typename stepper_t>
        auto makeConstantFieldPropagator(std::shared_ptr<const Acts::TrackingGeometry> geo, double bz);

        void Configure();

        void ChangeRun(int32_t runNumber) override {/*nothin here*/};

        void Execute(int32_t runNumber, uint64_t evtNumber);

    private:
        std::shared_ptr<spdlog::logger> m_log;
        std::shared_ptr<const Acts::Logger> m_acts_logger{nullptr};
//        // std::shared_ptr<CKFTrackingFunction> m_trackFinderFunc;
//
//
        std::shared_ptr<Acts::TrackingGeometry> m_geometry{nullptr};
//        Acts::GeometryContext m_geoctx;
//        Acts::CalibrationContext m_calibctx;
//        Acts::MagneticFieldContext m_fieldctx;
//
//        Acts::MeasurementSelector::Config m_sourcelinkSelectorCfg;
//
//        /// Private access to the logging instance
//        const Acts::Logger& logger() const { return *m_acts_logger; }

    };

}