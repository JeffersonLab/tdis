#pragma once

#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/Surfaces/CylinderBounds.hpp>
#include <Acts/Utilities/CalibrationContext.hpp>
#include <ActsExamples/EventData/IndexSourceLink.hpp>
#include <ActsExamples/EventData/Measurement.hpp>

#include <Eigen/Cholesky>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace tdis::tracking {

// One greedy choice per cylinder: compare all candidate positions to the
// predicted state using chi2 = residual^T (H P H^T + V)^-1 residual.
// Keep the chosen source link for backward filtering and EDM provenance.
class LayerCandidateCalibrator {
public:
    explicit LayerCandidateCalibrator(const ActsExamples::MeasurementContainer& measurements)
        : m_measurements(measurements) {
        for (ActsExamples::Index i = 0; i < measurements.size(); ++i) {
            m_candidates[measurements.getMeasurement(i).geometryId()].push_back(i);
        }
    }

    void calibrate(const Acts::GeometryContext&, const Acts::CalibrationContext&,
                   const Acts::SourceLink& sourceLink,
                   Acts::VectorMultiTrajectory::TrackStateProxy state) const {
        const auto geometryId = sourceLink.get<ActsExamples::IndexSourceLink>().geometryId();
        const auto& candidates = m_candidates.at(geometryId);
        const auto& bounds = dynamic_cast<const Acts::CylinderBounds&>(state.referenceSurface().bounds());
        const double circumference = 2 * std::numbers::pi * bounds.get(Acts::CylinderBounds::eR);
        const Acts::Vector2 predicted = state.predicted().head<2>();

        auto selected = m_selected.find(geometryId);
        if (selected == m_selected.end()) {
            double bestChi2 = std::numeric_limits<double>::infinity();
            ActsExamples::Index bestIndex = candidates.front();
            for (const auto index : candidates) {
                const auto measurement = m_measurements.getMeasurement<2>(index);
                Acts::Vector2 residual = measurement.parameters() - predicted;
                residual[0] = std::remainder(residual[0], circumference);
                const Acts::SquareMatrix2 innovation =
                    state.predictedCovariance().topLeftCorner<2, 2>() + measurement.covariance();
                if (!residual.allFinite() || !innovation.allFinite()) {
                    continue;
                }
                const Eigen::LDLT<Acts::SquareMatrix2> decomposition(innovation);
                if (decomposition.info() != Eigen::Success || !decomposition.isPositive() ||
                    (decomposition.vectorD().array() <= 0).any()) {
                    continue;
                }
                const double chi2 = residual.dot(decomposition.solve(residual));
                if (std::isfinite(chi2) && chi2 >= 0 && chi2 < bestChi2) {
                    bestChi2 = chi2;
                    bestIndex = index;
                }
            }
            if (!std::isfinite(bestChi2)) {
                throw std::runtime_error("No finite measurement candidate on cylinder");
            }
            selected = m_selected.emplace(geometryId, bestIndex).first;
        }

        const auto measurement = m_measurements.getMeasurement<2>(selected->second);
        Acts::Vector2 calibrated = measurement.parameters();
        // Use the nearest equivalent R*phi across the -pi/pi seam for the update.
        calibrated[0] = predicted[0] + std::remainder(calibrated[0] - predicted[0], circumference);
        const Acts::SquareMatrix2 covariance = measurement.covariance();
        state.allocateCalibrated(calibrated, covariance);
        state.setProjectorSubspaceIndices(std::array{Acts::eBoundLoc0, Acts::eBoundLoc1});
        state.setUncalibratedSourceLink(Acts::SourceLink{
            ActsExamples::IndexSourceLink{geometryId, selected->second}});
    }

    const auto& selectedCandidates() const { return m_selected; }

private:
    const ActsExamples::MeasurementContainer& m_measurements;
    std::map<Acts::GeometryIdentifier, std::vector<ActsExamples::Index>> m_candidates;
    mutable std::map<Acts::GeometryIdentifier, ActsExamples::Index> m_selected;
};

} // namespace tdis::tracking
