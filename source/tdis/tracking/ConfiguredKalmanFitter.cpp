#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>
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

    struct SimpleReverseFilteringLogic {
        double momentumThreshold = 0;

        bool doBackwardFiltering(Acts::VectorMultiTrajectory::ConstTrackStateProxy trackState) const
        {
            auto momentum = std::abs(1 / trackState.filtered()[Acts::eBoundQOverP]);
            return (momentum <= momentumThreshold);
        }
    };

    using namespace ActsExamples;

    struct ConfiguredKalmanFitter final : public tdis::ConfiguredFitter {
        KalmanFitter fitter;
        DirectKalmanFitter directFitter;

        Acts::GainMatrixUpdater kfUpdater;
        Acts::GainMatrixSmoother kfSmoother;
        SimpleReverseFilteringLogic reverseFilteringLogic;

        bool multipleScattering = false;
        bool energyLoss = false;
        Acts::FreeToBoundCorrection freeToBoundCorrection;

        IndexSourceLink::SurfaceAccessor slSurfaceAccessor;

        ConfiguredKalmanFitter(KalmanFitter&& f, DirectKalmanFitter&& df, const Acts::TrackingGeometry& trkGeo):
            fitter(std::move(f)),
            directFitter(std::move(df)),
            slSurfaceAccessor{trkGeo}
        {
        }

        template <typename calibrator_t> auto
        makeKfOptions(const GeneralFitterOptions& options, const calibrator_t& calibrator) const
        {
            // Setup extensions
            Acts::KalmanFitterExtensions<Acts::VectorMultiTrajectory> extensions;
            extensions.updater.connect<&Acts::GainMatrixUpdater::operator()<Acts::VectorMultiTrajectory>>(&kfUpdater);
            extensions.smoother.connect<&Acts::GainMatrixSmoother::operator()<Acts::VectorMultiTrajectory>>(&kfSmoother);
            extensions.reverseFilteringLogic.connect<&SimpleReverseFilteringLogic::doBackwardFiltering>(&reverseFilteringLogic);

            // Create options
            Acts::KalmanFitterOptions<Acts::VectorMultiTrajectory> kfOptions(
                options.geoContext,
                options.magFieldContext,
                options.calibrationContext,
                extensions,
                options.propOptions,
                &(*options.referenceSurface));

            // Configure options
            kfOptions.referenceSurfaceStrategy = Acts::KalmanFitterTargetSurfaceStrategy::first;
            kfOptions.multipleScattering = multipleScattering;
            kfOptions.energyLoss = energyLoss;
            kfOptions.freeToBoundCorrection = freeToBoundCorrection;
            kfOptions.extensions.calibrator.connect<&calibrator_t::calibrate>(&calibrator);

            if (options.doRefit) {
                kfOptions.extensions.surfaceAccessor.connect<&tdis::RefittingCalibrator::accessSurface>();
            } else {
                kfOptions.extensions.surfaceAccessor.connect<&IndexSourceLink::SurfaceAccessor::operator()>(&slSurfaceAccessor);
            }

            return kfOptions;
        }

        TrackFitterResult operator()(const std::vector<Acts::SourceLink>& sourceLinks,
                                     const TrackParameters& initialParameters,
                                     const GeneralFitterOptions& options,
                                     const ActsExamples::MeasurementCalibratorAdapter& calibrator,
                                     TrackContainer& tracks) const override
        {
            const auto kfOptions = makeKfOptions(options, calibrator);
            return fitter.fit(
                sourceLinks.begin(),
                sourceLinks.end(),
                initialParameters,
                kfOptions,
                tracks);
        }

        TrackFitterResult operator()(const std::vector<Acts::SourceLink>& sourceLinks,
                                     const TrackParameters& initialParameters,
                                     const GeneralFitterOptions& options,
                                     const tdis::RefittingCalibrator& calibrator,
                                     const std::vector<const Acts::Surface*>& surfaceSequence,
                                     TrackContainer& tracks) const override
        {
            const auto kfOptions = makeKfOptions(options, calibrator);
            return directFitter.fit(
                sourceLinks.begin(),
                sourceLinks.end(),
                initialParameters,
                kfOptions,
                surfaceSequence,
                tracks);
        }
    };



std::shared_ptr<tdis::ConfiguredFitter> makeKalmanFitterFunction(
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
    std::shared_ptr<const Acts::MagneticFieldProvider> magneticField,
    bool multipleScattering,
    bool energyLoss, double reverseFilteringMomThreshold,
    Acts::FreeToBoundCorrection freeToBoundCorrection,
    const Acts::Logger& logger)
{


    // Stepper should be copied into the fitters
    const tdis::Stepper stepper(std::move(magneticField));

    // Standard fitter
    const auto& geo = *trackingGeometry;
    Acts::Navigator::Config cfg{std::move(trackingGeometry)};
    cfg.resolvePassive = false;
    cfg.resolveMaterial = true;
    cfg.resolveSensitive = true;

    Acts::Navigator navigator(cfg, logger.cloneWithSuffix("Navigator"));

    Propagator propagator(stepper, std::move(navigator), logger.cloneWithSuffix("Propagator"));
    KalmanFitter trackFitter(std::move(propagator), logger.cloneWithSuffix("Fitter"));

    // Direct fitter
    Acts::DirectNavigator directNavigator{logger.cloneWithSuffix("DirectNavigator")};
    DirectPropagator directPropagator(stepper, std::move(directNavigator), logger.cloneWithSuffix("DirectPropagator"));
    DirectKalmanFitter directTrackFitter(std::move(directPropagator), logger.cloneWithSuffix("DirectFitter"));

    // build the fitter function. owns the fitter object.
    auto fitterFunction = std::make_shared<ConfiguredKalmanFitter>(std::move(trackFitter), std::move(directTrackFitter), geo);
    fitterFunction->multipleScattering = multipleScattering;
    fitterFunction->energyLoss = energyLoss;
    fitterFunction->reverseFilteringLogic.momentumThreshold = reverseFilteringMomThreshold;
    fitterFunction->freeToBoundCorrection = freeToBoundCorrection;

    return fitterFunction;
}

}  // tdis namespace