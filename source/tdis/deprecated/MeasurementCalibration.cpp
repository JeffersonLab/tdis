// This file is part of the ACTS project.
//
// Copyright (C) 2016 CERN for the benefit of the ACTS project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "../tracking/MeasurementCalibration.h"

#include <cassert>

#include "Acts/Definitions/TrackParametrization.hpp"
#include "Acts/EventData/SourceLink.hpp"
#include "ActsExamples/EventData/IndexSourceLink.hpp"
#include "ActsExamples/EventData/Measurement.hpp"
#include "ConfiguredKalmanFitter.h"

namespace Acts {
class VectorMultiTrajectory;
}  // namespace Acts

using namespace ActsExamples;

// TODO use ACTSExamples
void tdis::PassThroughCalibrator::calibrate(
    const ActsExamples::MeasurementContainer& measurements,
    const ClusterContainer* /*clusters*/, const Acts::GeometryContext& /*gctx*/,
    const Acts::CalibrationContext& /*cctx*/,
    const Acts::SourceLink& sourceLink,
    Acts::VectorMultiTrajectory::TrackStateProxy& trackState) const {
    trackState.setUncalibratedSourceLink(Acts::SourceLink{sourceLink});
    const IndexSourceLink& idxSourceLink = sourceLink.get<IndexSourceLink>();

    // assert((idxSourceLink.index() < measurements.size()) && "Source link index is outside the container bounds");

    const ConstVariableBoundMeasurementProxy measurement = measurements.getMeasurement(idxSourceLink.index());

    Acts::visit_measurement(measurement.size(), [&](auto N) -> void {
      constexpr std::size_t kMeasurementSize = decltype(N)::value;
      const ConstFixedBoundMeasurementProxy<kMeasurementSize> fixedMeasurement =
          static_cast<ConstFixedBoundMeasurementProxy<kMeasurementSize>>(
              measurement);

      trackState.allocateCalibrated(kMeasurementSize);
      trackState.calibrated<kMeasurementSize>() = fixedMeasurement.parameters();
      trackState.calibratedCovariance<kMeasurementSize>() =
          fixedMeasurement.covariance();
      trackState.setSubspaceIndices(fixedMeasurement.subspaceIndices());
    });
}

tdis::MeasurementCalibratorAdapter::MeasurementCalibratorAdapter(
    const MeasurementCalibrator& calibrator,
    const MeasurementContainer& measurements, const ClusterContainer* clusters)
    : m_calibrator{calibrator},
      m_measurements{measurements},
      m_clusters{clusters} {}

void tdis::MeasurementCalibratorAdapter::calibrate(
    const Acts::GeometryContext& gctx, const Acts::CalibrationContext& cctx,
    const Acts::SourceLink& sourceLink,
    Acts::VectorMultiTrajectory::TrackStateProxy trackState) const {
    return m_calibrator.calibrate(m_measurements, m_clusters, gctx, cctx,
                                  sourceLink, trackState);
}