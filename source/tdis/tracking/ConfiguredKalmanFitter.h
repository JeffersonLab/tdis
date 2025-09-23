# pragma once

#pragma once

#include <memory>
#include <vector>
#include "Acts/Definitions/Direction.hpp"
#include "Acts/Definitions/TrackParametrization.hpp"
#include "Acts/EventData/MultiTrajectory.hpp"
#include "Acts/EventData/TrackContainer.hpp"
#include "Acts/EventData/TrackStatePropMask.hpp"
#include "Acts/EventData/VectorMultiTrajectory.hpp"
#include "Acts/EventData/VectorTrackContainer.hpp"
#include "Acts/EventData/detail/CorrectedTransformationFreeToBound.hpp"
#include "Acts/Geometry/GeometryIdentifier.hpp"
#include "Acts/Propagator/DirectNavigator.hpp"
#include "Acts/Propagator/Navigator.hpp"
#include "Acts/Propagator/Propagator.hpp"
#include "Acts/Propagator/SympyStepper.hpp"
#include "Acts/TrackFitting/GainMatrixSmoother.hpp"
#include "Acts/TrackFitting/GainMatrixUpdater.hpp"
#include "Acts/TrackFitting/KalmanFitter.hpp"
#include "Acts/Utilities/Delegate.hpp"
#include "Acts/Utilities/Logger.hpp"
#include "ActsExamples/EventData/IndexSourceLink.hpp"
#include "ActsExamples/EventData/MeasurementCalibration.hpp"
#include "ActsExamples/EventData/Track.hpp"
#include "RefittingCalibrator.h"
#include "ConfiguredFitter.hpp"
namespace tdis {
class MeasurementCalibration;

/**
 * @brief Create a specialized track-fitter configuration with a KalmanFitter
 *        This struct implements the ConfiguredTrackFitter interface
 */
struct ConfiguredKalmanFitter final : public ConfiguredFitter
{

  /// Whether to include multiple scattering in the fit
  bool multipleScattering = false;

  /// Whether to include energy loss in the fit
  bool energyLoss = false;

  // Threshold below which we do "reverse filtering"
  // (implemented in the example logic)
  double reverseFilteringMomentumThreshold = 0.;

  // Correction used to handle non-linearities in free->bound transform
  Acts::FreeToBoundCorrection freeToBoundCorrection;

  // Ctors & dtors
  ConfiguredKalmanFitter(
      Acts::KalmanFitter<Acts::Propagator<Acts::SympyStepper, Acts::Navigator>, Acts::VectorMultiTrajectory>&& fitter,
      Acts::KalmanFitter<Acts::Propagator<Acts::SympyStepper, Acts::DirectNavigator>, Acts::VectorMultiTrajectory>&& directFitter,
      const Acts::TrackingGeometry& trkGeo);

  ~ConfiguredKalmanFitter() override = default;

  // Overridden from ConfiguredTrackFitter
    TrackFitterResult
  operator()(const std::vector<Acts::SourceLink>& sourceLinks,
             const ActsExamples::TrackParameters& initialParameters,
             const GeneralFitterOptions& options,
             const ActsExamples::MeasurementCalibratorAdapter& calibrator,
             tdis::TrackContainer& tracks) const override;

    TrackFitterResult
  operator()(const std::vector<Acts::SourceLink>& sourceLinks,
             const ActsExamples::TrackParameters& initialParameters,
             const GeneralFitterOptions& options,
             const RefittingCalibrator& calibrator,
             const std::vector<const Acts::Surface*>& surfaceSequence,
             tdis::TrackContainer& tracks) const override;
};

/**
 * @brief Make the KalmanFitter-based ConfiguredTrackFitter
 *
 * @param trackingGeometry The global tracking geometry
 * @param magneticField    The magnetic field
 * @param multipleScattering Whether to consider multiple scattering
 * @param energyLoss         Whether to consider energy loss
 * @param reverseFilteringMomThreshold If momentum is below this, do reverse filtering
 * @param freeToBoundCorrection Correction for non-linear free->bound transform
 * @param logger A logger for debug messages
 *
 * @return A shared_ptr to a ConfiguredTrackFitter that uses the Acts KalmanFitter
 */
std::shared_ptr<ConfiguredFitter> makeKalmanFitterFunction(
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
    std::shared_ptr<const Acts::MagneticFieldProvider> magneticField,
    bool multipleScattering,
    bool energyLoss,
    double reverseFilteringMomThreshold,
    Acts::FreeToBoundCorrection freeToBoundCorrection,
    const Acts::Logger& logger);

}  // namespace ActsExamples
