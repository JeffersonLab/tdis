#include "KalmanFittingFactory.h"

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Definitions/TrackParametrization.hpp>
#include <Acts/Definitions/Units.hpp>
#include <Acts/EventData/GenericBoundTrackParameters.hpp>
#include <Acts/EventData/MultiTrajectory.hpp>
#include <Acts/EventData/ParticleHypothesis.hpp>
#include <Acts/EventData/ProxyAccessor.hpp>
#include <Acts/EventData/SourceLink.hpp>
#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/TrackProxy.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/MagneticField/ConstantBField.hpp>

#include "extensions/spdlog/SpdlogToActs.h"
//#include <Acts/Propagator/AbortList.hpp>
#include <Acts/Propagator/EigenStepper.hpp>
#include <Acts/Propagator/MaterialInteractor.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/Propagator/StandardAborters.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>
#include <Acts/Surfaces/Surface.hpp>
#include <Acts/TrackFitting/GainMatrixSmoother.hpp>
#include <Acts/TrackFitting/GainMatrixUpdater.hpp>
#include <Acts/Utilities/Logger.hpp>
#include <Acts/Utilities/TrackHelpers.hpp>
#include <ActsExamples/EventData/IndexSourceLink.hpp>
#include <ActsExamples/EventData/Measurement.hpp>
#include <ActsExamples/EventData/MeasurementCalibration.hpp>
#include <podio_model/TrackParametersCollection.h>

#include <ActsExamples/EventData/Track.hpp>
//#include <edm4eic/Cov3f.h>
//#include <edm4eic/Cov6f.h>
//#include <edm4eic/Measurement2DCollection.h>
//#include <edm4eic/TrackParametersCollection.h>
//#include <edm4hep/Vector2f.h>
#include <fmt/core.h>
#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <list>
#include <optional>
#include <utility>

#include <extensions/spdlog/SpdlogToActs.h>

#include "TrackFitterFunction.hpp"


namespace ActsExamples {
    class TrackFitterFunction;
}

void tdis::tracking::KalmanFittingFactory::Configure()
{

    m_log = m_service_log().logger("tracking:ckf");
    m_acts_logger = makeActsLogger("CKF", m_log);

//    m_geoSvc = m_serviceGeometry->GetActsGeometryContext();
//
//    m_BField = std::dynamic_pointer_cast<const eicrecon::BField::DD4hepBField>(m_geoSvc->getFieldProvider());
//    m_fieldctx = eicrecon::BField::BFieldVariant(m_BField);
//
//    // eta bins, chi2 and #sourclinks per surface cutoffs
//    m_sourcelinkSelectorCfg = {
//        {Acts::GeometryIdentifier(),
//         {m_cfg.etaBins, m_cfg.chi2CutOff,
//          {m_cfg.numMeasurementsCutOff.begin(), m_cfg.numMeasurementsCutOff.end()}
//         }
//        },
//    };
//    m_trackFinderFunc = CKFTracking::makeCKFTrackingFunction(m_geoSvc->trackingGeometry(), m_BField, logger());

}

void tdis::tracking::KalmanFittingFactory::Execute(int32_t runNumber, uint64_t evtNumber)
{

    m_log->debug("{}::Execute", this->GetTypeName());

    std::shared_ptr<ActsExamples::TrackFitterFunction> fit;

    // Construct a perigee surface as the target surface
    auto pSurface = Acts::Surface::makeShared<Acts::PerigeeSurface>(
        Acts::Vector3{0., 0., 0.});

    // Type erased calibrator for the measurements
    std::shared_ptr<ActsExamples::MeasurementCalibrator> calibrator;

    double bz = 1.5 * Acts::UnitConstants::T;

    auto field = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0.0, 0.0, bz));


    // Construct the MagneticFieldContext using the cache
    Acts::MagneticFieldContext magFieldContext;
    // Create a cache for the magnetic field
    //Acts::ConstantBField::Cache magFieldCache = field->makeCache(magFieldContext);
    Acts::CalibrationContext calibrationContext;

    ActsExamples::TrackFitterFunction::GeneralFitterOptions options{
        m_serviceGeometry->GetActsGeometryContext(),
        magFieldContext, calibrationContext, pSurface.get(),
        Acts::PropagatorPlainOptions(m_serviceGeometry->GetActsGeometryContext(), magFieldContext)};

    auto trackContainer = std::make_shared<Acts::VectorTrackContainer>();
    auto trackStateContainer = std::make_shared<Acts::VectorMultiTrajectory>();
    Acts::TrackContainer tracks(trackContainer, trackStateContainer);

    // Perform the fit for each input track
    std::vector<Acts::SourceLink> trackSourceLinks;

    for (auto track: *m_parameters_input()) {
        track.

    }

    // // Clear & reserve the right size
    // trackSourceLinks.clear();
    // trackSourceLinks.reserve(protoTrack.size());
    //
    // // Fill the source links via their indices from the container
    // for (auto measIndex : protoTrack) {
    //     ConstVariableBoundMeasurementProxy measurement =
    //         measurements.getMeasurement(measIndex);
    //     IndexSourceLink sourceLink(measurement.geometryId(), measIndex);
    //     trackSourceLinks.push_back(Acts::SourceLink(sourceLink));
    // }
    //
    // ACTS_DEBUG("Invoke direct fitter for track " << itrack);
    // auto result = (*m_cfg.fit)(trackSourceLinks, initialParams, options,
    //                            calibrator, tracks);
    //
    // if (result.ok()) {
    //     // Get the fit output object
    //     const auto& track = result.value();
    //     if (track.hasReferenceSurface()) {
    //         ACTS_VERBOSE("Fitted parameters for track " << itrack);
    //         ACTS_VERBOSE("  " << track.parameters().transpose());
    //     } else {
    //         ACTS_DEBUG("No fitted parameters for track " << itrack);
    //     }
    // } else {
    //     ACTS_WARNING("Fit failed for track "
    //                  << itrack << " with error: " << result.error() << ", "
    //                  << result.error().message());
    // }
//}



}

template <typename stepper_t>
auto tdis::tracking::KalmanFittingFactory::makeConstantFieldPropagator(std::shared_ptr<const Acts::TrackingGeometry> geo, double bz) {
    Acts::Navigator::Config cfg{std::move(geo)};
    cfg.resolvePassive = false;
    cfg.resolveMaterial = true;
    cfg.resolveSensitive = true;
    Acts::Navigator navigator(cfg, Acts::getDefaultLogger("Navigator", Acts::Logging::INFO));
    auto field = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0.0, 0.0, bz));
    stepper_t stepper(std::move(field));
    return Acts::Propagator<decltype(stepper), Acts::Navigator>(std::move(stepper), std::move(navigator));;
}
