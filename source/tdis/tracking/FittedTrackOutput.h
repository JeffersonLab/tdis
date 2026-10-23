#pragma once

#include <Acts/Definitions/Units.hpp>
#include <Acts/EventData/TrackParameters.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/Propagator/SympyStepper.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>
#include <system_error>

namespace tdis::tracking {

using FieldPropagator = Acts::Propagator<Acts::SympyStepper>;

// Field-only extrapolation: no material corrections or tracking-volume lookup.
inline Acts::Result<Acts::BoundTrackParameters> extrapolateToPerigee(
    const Acts::BoundTrackParameters& fitted, const FieldPropagator& propagator,
    const Acts::GeometryContext& gctx, const Acts::MagneticFieldContext& mctx,
    double pathLimit = 10 * Acts::UnitConstants::m) {
    auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(Acts::Vector3::Zero());
    FieldPropagator::Options<> options(gctx, mctx);
    options.direction = Acts::Direction::Backward();
    options.pathLimit = pathLimit;
    auto result = propagator.propagate(fitted, *perigee, options);
    if (!result.ok()) {
        return result.error();
    }
    const auto& end = result.value().endParameters;
    if (!end || !end->parameters().allFinite() || end->qOverP() == 0 ||
        !end->covariance() || !end->covariance()->allFinite()) {
        return std::make_error_code(std::errc::result_out_of_range);
    }
    return *end;
}

// Rows are Cartesian (mm, GeV); columns use ACTS bound units, including c*t.
inline Acts::ActsSquareMatrix<6> boundToCartesianJacobian(
    const Acts::BoundTrackParameters& fitted, const Acts::GeometryContext& gctx) {
    const auto boundToFree = fitted.referenceSurface().boundToFreeJacobian(
        gctx, fitted.position(gctx), fitted.direction());
    Acts::ActsMatrix<6, 8> freeToCartesian = Acts::ActsMatrix<6, 8>::Zero();
    freeToCartesian.block<3, 3>(0, Acts::eFreePos0) =
        Acts::SquareMatrix3::Identity() / Acts::UnitConstants::mm;
    freeToCartesian.block<3, 3>(3, Acts::eFreeDir0) =
        Acts::SquareMatrix3::Identity() * fitted.absoluteMomentum() / Acts::UnitConstants::GeV;
    freeToCartesian.block<3, 1>(3, Acts::eFreeQOverP) =
        -fitted.momentum() / (fitted.qOverP() * Acts::UnitConstants::GeV);
    return freeToCartesian * boundToFree;
}

} // namespace tdis::tracking
