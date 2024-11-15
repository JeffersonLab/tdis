// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck, Sylvester Joosten

#include <Acts/Definitions/Direction.hpp>
#include <Acts/Definitions/TrackParametrization.hpp>
#include <Acts/EventData/ParticleHypothesis.hpp>
#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/TrackStatePropMask.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Geometry/Layer.hpp>
#include <Acts/Geometry/TrackingGeometry.hpp>
#include <Acts/Geometry/TrackingVolume.hpp>
#include <Acts/MagneticField/MagneticFieldProvider.hpp>
#include <Acts/Propagator/EigenStepper.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/TrackFinding/CombinatorialKalmanFilter.hpp>
#include <Acts/TrackFitting/GainMatrixSmoother.hpp>
#include <Acts/TrackFitting/GainMatrixUpdater.hpp>
#include <Acts/Utilities/Logger.hpp>
#include <ActsExamples/EventData/IndexSourceLink.hpp>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <boost/container/vector.hpp>
#include <memory>
#include <utility>

#include "ActsExamples/EventData/Track.hpp"
// #include "CKFTracking.h"

namespace tdis {



    using Updater = Acts::GainMatrixUpdater;
    using Smoother = Acts::GainMatrixSmoother;

    using Stepper = Acts::EigenStepper<>;
    using Navigator = Acts::Navigator;
    using Propagator = Acts::Propagator<Stepper, Navigator>;

    using CombinatorialKalmanFilter = Acts::CombinatorialKalmanFilter<Propagator, Acts::VectorMultiTrajectory>;

    using TrackContainer = Acts::TrackContainer<Acts::VectorTrackContainer,
                                                Acts::VectorMultiTrajectory, std::shared_ptr>;

    /// Track finder function that takes input measurements, initial track state
    /// and track finder options and returns some track-finder-specific result.
    using TrackFinderOptions = Acts::CombinatorialKalmanFilterOptions<ActsExamples::IndexSourceLinkAccessor::Iterator, Acts::VectorMultiTrajectory>;
    using TrackFinderResult = Acts::Result<std::vector<ActsExamples::TrackContainer::TrackProxy>>;

    /// Find function that takes the above parameters
    /// @note This is separated into a virtual interface to keep compilation units
    /// small
    class CKFTrackingFunction {
    public:
        virtual ~CKFTrackingFunction() = default;

        virtual TrackFinderResult operator()(const ActsExamples::TrackParameters&, const TrackFinderOptions&, ActsExamples::TrackContainer&) const = 0;
    };

    /// Create the track finder function implementation.
    /// The magnetic field is intentionally given by-value since the variantresults
    /// contains shared_ptr anyways.
    static std::shared_ptr<CKFTrackingFunction> makeCKFTrackingFunction(
            std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
            std::shared_ptr<const Acts::MagneticFieldProvider> magneticField,
            const Acts::Logger& logger);

    /** Finder implementation .
     *
     * \ingroup track
     */
    struct CKFTrackingFunctionImpl : public tdis::CKFTrackingFunction {
        CombinatorialKalmanFilter trackFinder;

        CKFTrackingFunctionImpl(CombinatorialKalmanFilter&& f) : trackFinder(std::move(f)) {}

        tdis::TrackFinderResult operator()(
            const ActsExamples::TrackParameters& initialParameters,
            const tdis::TrackFinderOptions& options,
            TrackContainer& tracks) const override {
            return trackFinder.findTracks(initialParameters, options, tracks);
        };
    };

    std::shared_ptr<tdis::CKFTrackingFunction> makeCKFTrackingFunction(
        std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
        std::shared_ptr<const Acts::MagneticFieldProvider> magneticField,
        const Acts::Logger& logger)
    {
        Stepper stepper(std::move(magneticField));
        Navigator::Config cfg{trackingGeometry};
        cfg.resolvePassive = false;
        cfg.resolveMaterial = true;
        cfg.resolveSensitive = true;
        Navigator navigator(cfg);

        Propagator propagator(std::move(stepper), std::move(navigator));
        CombinatorialKalmanFilter trackFinder(std::move(propagator), logger.cloneWithSuffix("CKF"));

        // build the track finder functions. owns the track finder object.
        return std::make_shared<CKFTrackingFunctionImpl>(std::move(trackFinder));
    }

}  // namespace tdis
