#include "tracking/LayerCandidateCalibrator.h"

#include <Acts/Surfaces/CylinderSurface.hpp>
#include <iostream>

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void checkSelection(bool reversed, bool covarianceWeighted, bool seam) {
    Acts::GeometryContext gctx;
    Acts::CalibrationContext cctx;
    auto surface = Acts::Surface::makeShared<Acts::CylinderSurface>(
        Acts::Transform3::Identity(), std::make_shared<Acts::CylinderBounds>(100., 275.));
    const auto geometryId = Acts::GeometryIdentifier{}.withVolume(1).withLayer(2).withSensitive(1);
    surface->assignGeometryId(geometryId);

    ActsExamples::MeasurementContainer measurements;
    // Same pad (same u), different z: the second candidate matches the prediction.
    Acts::Vector2 poor{10., 20.};
    Acts::Vector2 best{10., 3.};
    Acts::Vector2 predicted{10., 3.};
    Acts::SquareMatrix2 poorCov = Acts::SquareMatrix2::Identity();
    Acts::SquareMatrix2 bestCov = Acts::SquareMatrix2::Identity();
    if (covarianceWeighted) {
        poor[1] = 4.;
        best[1] = 6.;
        bestCov(1, 1) = 100.; // Larger residual, but smaller innovation chi2.
    }
    if (seam) {
        predicted[0] = 100 * std::numbers::pi - 0.1;
        poor[0] = predicted[0] - 3.;
        poor[1] = 3.;
        best[0] = -100 * std::numbers::pi + 0.1;
    }
    auto add = [&](const Acts::Vector2& pos, const Acts::SquareMatrix2& cov) {
        measurements.emplaceMeasurement<2>(geometryId,
            std::array{Acts::eBoundLoc0, Acts::eBoundLoc1}, pos, cov);
    };
    if (reversed) { add(best, bestCov); add(poor, poorCov); }
    else { add(poor, poorCov); add(best, bestCov); }

    Acts::VectorMultiTrajectory states;
    auto state = states.makeTrackState();
    state.setReferenceSurface(surface);
    state.predicted().setZero();
    state.predicted().head<2>() = predicted;
    state.predictedCovariance().setIdentity();
    const Acts::SourceLink representative{ActsExamples::IndexSourceLink{geometryId, 0}};
    tdis::tracking::LayerCandidateCalibrator calibrator(measurements);
    calibrator.calibrate(gctx, cctx, representative, state);
    const auto chosen = state.getUncalibratedSourceLink().get<ActsExamples::IndexSourceLink>().index();
    require(chosen == (reversed ? 0 : 1), "Wrong candidate chosen");
    require(measurements.size() == 2, "Candidate was removed");
    require(std::abs(state.calibrated<2>()[1] - best[1]) < 1e-10, "Wrong z calibrated");
    if (seam) {
        require(std::abs(state.calibrated<2>()[0] - predicted[0] - 0.2) < 1e-10,
                "Cylinder seam was not wrapped");
    }
    // Backward filtering must keep the original measurement association.
    state.predicted().head<2>() = poor;
    calibrator.calibrate(gctx, cctx, representative, state);
    require(state.getUncalibratedSourceLink().get<ActsExamples::IndexSourceLink>().index() == chosen,
            "Backward filtering changed the selected candidate");
}

void checkInnovationGate() {
    Acts::VectorMultiTrajectory states;
    auto state = states.makeTrackState();
    state.predicted().setZero();
    state.predictedCovariance().setIdentity();
    Acts::VectorMultiTrajectory::ConstTrackStateProxy constState{state};
    // Innovation S = [[2,1],[1,2]], r=(3,3), so chi2=6 exactly.
    Acts::SquareMatrix2 covariance;
    covariance << 1., 1., 1., 1.;
    state.allocateCalibrated(Acts::Vector2{3., 3.}, covariance);
    tdis::tracking::InnovationOutlierFinder boundary(6.);
    tdis::tracking::InnovationOutlierFinder tighter(5.99);
    tdis::tracking::InnovationOutlierFinder disabled(0.);
    require(!boundary(constState), "Gate rejects an inclusive chi2 boundary");
    require(tighter(constState), "Gate ignores correlated innovation chi2");
    require(!disabled(constState), "Zero cut must disable finite-chi2 rejection");
    // Increasing prediction uncertainty must reduce chi2 (use P+V, not V).
    state.predictedCovariance() *= 100.;
    require(!tighter(constState), "Gate ignores prediction uncertainty");
    state.predictedCovariance()(0, 0) = NAN;
    require(disabled(constState), "Non-finite innovation was accepted");
    state.predictedCovariance().setZero();
    require(disabled(constState), "Singular innovation was accepted");
    state.predictedCovariance().setIdentity();
    state.predictedCovariance()(0, 0) = -10.;
    require(disabled(constState), "Indefinite innovation was accepted");
}

int main() {
    checkSelection(false, false, false);
    checkSelection(true, false, false);
    checkSelection(false, true, false);
    checkSelection(false, false, true);
    checkInnovationGate();
    std::cout << "PASS: repeated pad candidates, input-order reversal, covariance weighting, cylinder seam, retained association\n";
}
