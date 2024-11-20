#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>

#include <ActsExamples/EventData/Track.hpp>

namespace tdis::tracking {



    struct KalmanFittingFactory : public JOmniFactory<KalmanFittingFactory> {

        PodioInput<tdis::DigitizedMtpcMcHit> m_mc_hits_in{this, {"DigitizedMtpcMcHit"}};
        PodioOutput<edm4eic::TrackerHit> m_tracker_hits_out{this, "TrackerHit"};
        PodioOutput<edm4eic::Measurement2D> m_measurements_out{this, "Measurement2D"};
        Service<ActsGeometryService> m_service_geometry{this};
        Service<services::LogService> m_service_log{this};
        Parameter<bool> m_cfg_use_true_pos{this, "acts:use_true_position", true, "Use true hits xyz instead of digitized one"};


        // Construct a propagator using a constant magnetic field along z.
        template <typename stepper_t>
        auto makeConstantFieldPropagator(
            std::shared_ptr<const Acts::TrackingGeometry> geo, double bz) {
            Acts::Navigator::Config cfg{std::move(geo)};
            cfg.resolvePassive = false;
            cfg.resolveMaterial = true;
            cfg.resolveSensitive = true;
            Acts::Navigator navigator(
                cfg, Acts::getDefaultLogger("Navigator", Acts::Logging::INFO));
            auto field =
                std::make_shared<Acts::ConstantBField>(Acts::Vector3(0.0, 0.0, bz));
            stepper_t stepper(std::move(field));
            return Acts::Propagator<decltype(stepper), Acts::Navigator>(
                std::move(stepper), std::move(navigator));
        }

        void Configure() {
            m_log = log;
            m_acts_logger = eicrecon::getSpdlogLogger("CKF", m_log);

            m_geoSvc = geo_svc;

            m_BField = std::dynamic_pointer_cast<const eicrecon::BField::DD4hepBField>(m_geoSvc->getFieldProvider());
            m_fieldctx = eicrecon::BField::BFieldVariant(m_BField);

            // eta bins, chi2 and #sourclinks per surface cutoffs
            m_sourcelinkSelectorCfg = {
                {Acts::GeometryIdentifier(),
                 {m_cfg.etaBins, m_cfg.chi2CutOff,
                  {m_cfg.numMeasurementsCutOff.begin(), m_cfg.numMeasurementsCutOff.end()}
                 }
                },
        };
            m_trackFinderFunc = CKFTracking::makeCKFTrackingFunction(m_geoSvc->trackingGeometry(), m_BField, logger());
        }

        void ChangeRun(int32_t /*run_nr*/) {
        }

        void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {


        }
    private:
        std::shared_ptr<spdlog::logger> m_log;
        std::shared_ptr<const Acts::Logger> m_acts_logger{nullptr};
        std::shared_ptr<CKFTrackingFunction> m_trackFinderFunc;
        std::shared_ptr<const ActsGeometryProvider> m_geoSvc;

        std::shared_ptr<const eicrecon::BField::DD4hepBField> m_BField = nullptr;
        Acts::GeometryContext m_geoctx;
        Acts::CalibrationContext m_calibctx;
        Acts::MagneticFieldContext m_fieldctx;

        Acts::MeasurementSelector::Config m_sourcelinkSelectorCfg;

        /// Private access to the logging instance
        const Acts::Logger& logger() const { return *m_acts_logger; }

    };

}