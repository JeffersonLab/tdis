#include "tracking/FittedTrackOutput.h"

#include <Acts/MagneticField/ConstantBField.hpp>
#include <Acts/Surfaces/CylinderSurface.hpp>
#include <Eigen/Eigenvalues>
#include <iostream>

using namespace tdis::tracking;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Acts::BoundSquareMatrix testCovariance() {
    Acts::BoundSquareMatrix root = Acts::BoundSquareMatrix::Identity();
    root(2, 0) = 0.003;
    root(3, 1) = -0.002;
    root(4, 0) = 0.001;
    root(4, 2) = 0.002;
    root(5, 1) = 0.01;
    root.diagonal() << 0.1, 0.2, 0.01, 0.01, 0.02, Acts::UnitConstants::ns;
    return root * root.transpose();
}

Acts::ActsVector<6> cartesian(const Acts::BoundTrackParameters& parameters,
                        const Acts::GeometryContext& gctx) {
    Acts::ActsVector<6> state;
    state.head<3>() = parameters.position(gctx) / Acts::UnitConstants::mm;
    state.tail<3>() = parameters.momentum() / Acts::UnitConstants::GeV;
    return state;
}

void checkJacobian(const Acts::BoundTrackParameters& parameters) {
    Acts::GeometryContext gctx;
    const auto jacobian = boundToCartesianJacobian(parameters, gctx);
    Acts::ActsSquareMatrix<6> numerical;
    for (int column = 0; column < 6; ++column) {
        const double step = column < 2 ? 1e-4 : 1e-6;
        auto plus = parameters.parameters();
        auto minus = plus;
        plus[column] += step;
        minus[column] -= step;
        const auto surface = parameters.referenceSurface().getSharedPtr();
        const Acts::BoundTrackParameters high(surface, plus, parameters.covariance(), parameters.particleHypothesis());
        const Acts::BoundTrackParameters low(surface, minus, parameters.covariance(), parameters.particleHypothesis());
        numerical.col(column) = (cartesian(high, gctx) - cartesian(low, gctx)) / (2 * step);
    }
    require((numerical - jacobian).cwiseAbs().maxCoeff() < 1e-6,
            "Cartesian Jacobian disagrees with finite differences");
    const Acts::ActsSquareMatrix<6> covariance = jacobian * *parameters.covariance() * jacobian.transpose();
    require(covariance.allFinite() && covariance.isApprox(covariance.transpose()),
            "Cartesian covariance is non-finite or asymmetric");
    Eigen::SelfAdjointEigenSolver<Acts::ActsSquareMatrix<6>> eigenvalues(covariance);
    require(eigenvalues.eigenvalues().minCoeff() > -1e-10, "Cartesian covariance is not positive semidefinite");
}

void checkPropagation(double fieldTesla, double chargeSign) {
    Acts::GeometryContext gctx;
    Acts::MagneticFieldContext mctx;
    auto field = std::make_shared<Acts::ConstantBField>(
        Acts::Vector3(0, 0, fieldTesla * Acts::UnitConstants::T));
    FieldPropagator propagator{Acts::SympyStepper(field)};
    auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(Acts::Vector3::Zero());
    auto cylinder = Acts::Surface::makeShared<Acts::CylinderSurface>(
        Acts::Transform3::Identity(), std::make_shared<Acts::CylinderBounds>(60., 275.));
    Acts::BoundVector vector;
    vector << 1.5, 12., 0.4, 1.2, chargeSign / 0.8, 3. * Acts::UnitConstants::ns;
    const Acts::BoundTrackParameters initial(perigee, vector, testCovariance(), Acts::ParticleHypothesis::proton());
    FieldPropagator::Options<> options(gctx, mctx);
    auto outward = propagator.propagate(initial, *cylinder, options);
    require(outward.ok() && outward.value().endParameters.has_value(), "Outward cylinder propagation failed");
    const auto& atCylinder = *outward.value().endParameters;
    checkJacobian(initial);
    checkJacobian(atCylinder);
    const auto inward = extrapolateToPerigee(atCylinder, propagator, gctx, mctx);
    require(inward.ok(), "Inward perigee extrapolation failed");
    const auto& returned = inward.value();
    std::cout << "Bz=" << fieldTesla << " charge=" << chargeSign
              << " round-trip parameter residuals: "
              << (returned.parameters() - initial.parameters()).transpose() << std::endl;
    // ACTS stops within its surface tolerance; compare time in ns, not c*t.
    auto residual = (returned.parameters() - initial.parameters()).eval();
    residual[Acts::eBoundTime] /= Acts::UnitConstants::ns;
    require(residual.cwiseAbs().maxCoeff() < 1e-4,
            "Perigee parameters failed field-only round trip");
    require(returned.covariance()->isApprox(*initial.covariance(), 1e-6),
            "Perigee covariance failed field-only round trip");
    const auto position = returned.position(gctx);
    const auto direction = returned.direction();
    require(std::abs(position.x() * direction.x() + position.y() * direction.y()) < 1e-4,
            "Returned position is not at transverse closest approach");
    require(returned.time() < atCylinder.time(), "Backward propagation did not reduce time");
    require(!extrapolateToPerigee(atCylinder, propagator, gctx, mctx, 0.).ok(),
            "Unreachable perigee must report failure for cylinder fallback");
}

int main() {
    for (const double field : {0., 4.7}) {
        for (const double sign : {-1., 1.}) {
            checkPropagation(field, sign);
        }
    }
    std::cout << "PASS: cylinder/perigee Jacobians, both charge signs, covariance round trips, closest approach, time, extrapolation failure\n";
}
