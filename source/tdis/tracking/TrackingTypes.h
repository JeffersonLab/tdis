//
// Created by romanov on 9/23/2025.
// Subject to the terms in the LICENSE file found in the top-level directory.
//

#pragma once

namespace Acts {
    class SympyStepper;
}

namespace tdis {
    using Stepper = Acts::SympyStepper;
    using Propagator = Acts::Propagator<Stepper, Acts::Navigator>;
    using KalmanFitter = Acts::KalmanFitter<Propagator, Acts::VectorMultiTrajectory>;
    using DirectPropagator = Acts::Propagator<Stepper, Acts::DirectNavigator>;
    using DirectKalmanFitter = Acts::KalmanFitter<DirectPropagator, Acts::VectorMultiTrajectory>;

    using TrackContainer = Acts::TrackContainer<Acts::MutablePodioTrackContainer, Acts::MutablePodioTrackStateContainer>;
    using TrackFitterResult = Acts::Result<TrackContainer::TrackProxy>;
}