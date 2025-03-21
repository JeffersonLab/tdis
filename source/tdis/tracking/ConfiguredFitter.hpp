#pragma once

#include <functional>

#include <Acts/EventData/SourceLink.hpp>
#include <Acts/EventData/TrackParameters.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Geometry/GeometryContext.hpp>
#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/MagneticField/MagneticFieldContext.hpp>
#include <Acts/MagneticField/MagneticFieldProvider.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/TrackFitting/BetheHeitlerApprox.hpp>
#include <Acts/TrackFitting/GsfOptions.hpp>
#include <Acts/Utilities/CalibrationContext.hpp>
#include <ActsExamples/EventData/MeasurementCalibration.hpp>
#include <ActsExamples/EventData/Measurement.hpp>

#include <ActsExamples/EventData/Track.hpp>
#include "RefittingCalibrator.h"

namespace ActsExamples {

/// Fit function that takes the above parameters and runs a fit
/// @note This is separated into a virtual interface to keep compilation units
/// small.
class ConfiguredFitter {
 public:
  using TrackFitterResult = Acts::Result<TrackContainer::TrackProxy>;

  struct GeneralFitterOptions {
    std::reference_wrapper<const Acts::GeometryContext> geoContext;
    std::reference_wrapper<const Acts::MagneticFieldContext> magFieldContext;
    std::reference_wrapper<const Acts::CalibrationContext> calibrationContext;
    const Acts::Surface* referenceSurface = nullptr;
    Acts::PropagatorPlainOptions propOptions;
    bool doRefit = false;
  };

  virtual ~ConfiguredFitter() = default;

  virtual TrackFitterResult operator()(const std::vector<Acts::SourceLink>&,
                                       const TrackParameters&,
                                       const GeneralFitterOptions&,
                                       const ActsExamples::MeasurementCalibratorAdapter&,
                                       TrackContainer&) const = 0;

  virtual TrackFitterResult operator()(const std::vector<Acts::SourceLink>&,
                                       const TrackParameters&,
                                       const GeneralFitterOptions&,
                                       const RefittingCalibrator&,
                                       const std::vector<const Acts::Surface*>&,
                                       TrackContainer&) const = 0;
};

/// Makes a fitter function object for the Kalman Filter
///
std::shared_ptr<ConfiguredFitter> makeKalmanFitterFunction(
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
    std::shared_ptr<const Acts::MagneticFieldProvider> magneticField,
    bool multipleScattering = true,
    bool energyLoss = true,
    double reverseFilteringMomThreshold = 0.0,
    Acts::FreeToBoundCorrection freeToBoundCorrection = Acts::FreeToBoundCorrection(),
    const Acts::Logger& logger = *Acts::getDefaultLogger("Kalman", Acts::Logging::INFO)
);

/// This type is used in the Examples framework for the Bethe-Heitler
/// approximation
using BetheHeitlerApprox = Acts::AtlasBetheHeitlerApprox<6, 5>;

/// Available algorithms for the mixture reduction
enum class MixtureReductionAlgorithm { weightCut, KLDistance };

/// Makes a fitter function object for the GSF
///
/// @param trackingGeometry the trackingGeometry for the propagator
/// @param magneticField the magnetic field for the propagator
/// @param betheHeitlerApprox The object that encapsulates the approximation.
/// @param maxComponents number of maximum components in the track state
/// @param weightCutoff when to drop components
/// @param componentMergeMethod How to merge a mixture to a single set of
/// parameters and covariance
/// @param mixtureReductionAlgorithm How to reduce the number of components
/// in a mixture
/// @param logger a logger instance
std::shared_ptr<ConfiguredFitter> makeGsfFitterFunction(
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
    std::shared_ptr<const Acts::MagneticFieldProvider> magneticField,
    BetheHeitlerApprox betheHeitlerApprox, std::size_t maxComponents,
    double weightCutoff, Acts::ComponentMergeMethod componentMergeMethod,
    MixtureReductionAlgorithm mixtureReductionAlgorithm,
    const Acts::Logger& logger);

/// Makes a fitter function object for the Global Chi Square Fitter (GX2F)
///
/// @param trackingGeometry the trackingGeometry for the propagator
/// @param magneticField the magnetic field for the propagator
/// @param multipleScattering bool
/// @param energyLoss bool
/// @param freeToBoundCorrection bool
/// @param nUpdateMax max number of iterations during the fit
/// @param relChi2changeCutOff Check for convergence (abort condition). Set to 0 to skip.
/// @param logger a logger instance
std::shared_ptr<ConfiguredFitter> makeGlobalChiSquareFitterFunction(
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
    std::shared_ptr<const Acts::MagneticFieldProvider> magneticField,
    bool multipleScattering = true, bool energyLoss = true,
    Acts::FreeToBoundCorrection freeToBoundCorrection =
        Acts::FreeToBoundCorrection(),
    std::size_t nUpdateMax = 5, double relChi2changeCutOff = 1e-7,
    const Acts::Logger& logger = *Acts::getDefaultLogger("Gx2f",
                                                         Acts::Logging::INFO));

}  // namespace ActsExamples
