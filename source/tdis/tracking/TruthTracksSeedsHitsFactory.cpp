#include "TruthTracksSeedsHitsFactory.h"

#include <cmath>
#include <stdexcept>

#include <podio_model/DigitizedMtpcMcTrack.h>
#include <podio_model/DigitizedMtpcMcTrackCollection.h>

#include <Acts/Definitions/Units.hpp>
#include <Acts/Surfaces/CylinderBounds.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>

#include "geometry/TdisGeometryHelper.hpp"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrackSeedCollection.h"
#include "podio_model/TrackerHitCollection.h"

// ---------------------------------------------------------------------------
// Helper: pixel-size → spatial resolution (σ = pitch / √12)
// ---------------------------------------------------------------------------
inline double get_resolution(const double pixel_size) {
    constexpr double sqrt_12 = 3.4641016151;
    return pixel_size / sqrt_12;
}

// Helper: pixel-size → variance (σ²)
inline double get_variance(const double pixel_size) {
    const double res = get_resolution(pixel_size);
    return res * res;
}

namespace tdis::tracking {

// ---------------------------------------------------------------------------
// Configure
// ---------------------------------------------------------------------------
void TruthTracksSeedsHitsFactory::Configure() {
    m_service_geometry();
    m_log = m_service_log->logger("TruthTracksSeedsHitsFactory");

    if (!std::isfinite(m_cfg_hitZVariance()) || m_cfg_hitZVariance() <= 0.0) {
        throw std::invalid_argument("acts:hit:cov:z must be finite and positive [mm^2]");
    }

    std::random_device rd;
    m_generator = std::mt19937(rd());
}

// ---------------------------------------------------------------------------
// generateNormal
// ---------------------------------------------------------------------------
double TruthTracksSeedsHitsFactory::generateNormal(double mean, double stddev) {
    std::normal_distribution<double> dist(mean, stddev);
    return dist(m_generator);
}

// ---------------------------------------------------------------------------
// createHitAndMeasurement
// ---------------------------------------------------------------------------
std::pair<std::optional<tdis::TrackerHit>, std::optional<tdis::Measurement2D>>
TruthTracksSeedsHitsFactory::createHitAndMeasurement(
    const tdis::DigitizedMtpcMcHit& mcHit,
    uint64_t                        event_index,
    const std::vector<double>&      plane_positions)
{
    namespace ActsUnits = Acts::UnitConstants;

    const int    ring      = mcHit.getRing();
    const int    pad       = mcHit.getPad();
    const int    plane     = mcHit.getPlane();

    if (plane < 0 || static_cast<size_t>(plane) >= plane_positions.size()) {
        m_log->warn("Event #{}: invalid readout plane {}, skipping hit", event_index, plane);
        return {std::nullopt, std::nullopt};
    }

    // Sentinel pad values indicate invalid / out-of-acceptance hits
    if (pad == -999 || pad == 999) {
        m_log->warn("Event #{}: MC hit has sentinel pad value {}, skipping", event_index, pad);
        return {std::nullopt, std::nullopt};
    }

    // -----------------------------------------------------------------------
    // Position selection
    // -----------------------------------------------------------------------
    // Use true position only when configured AND the true coords are finite
    const auto truePosition = mcHit.getTruePosition();
    const bool shouldUseTruePos =
        m_cfg_useTrueHitPos() && std::isfinite(truePosition.x) &&
        std::isfinite(truePosition.y) && std::isfinite(truePosition.z);

    Vector3f position;
    if (shouldUseTruePos) {
        position = truePosition;
    } else {
        const double z_to_gem = mcHit.getZToGem();
        if (!std::isfinite(z_to_gem)) {
            m_log->warn("Event #{}: non-finite zToGem for plane {}, skipping hit", event_index, plane);
            return {std::nullopt, std::nullopt};
        }
        auto [padX, padY] = getPadCenter(ring, pad);
        const double planeZ = plane_positions.at(plane);
        const double hitCalcZ = planeZ + (plane % 2 ? -z_to_gem : z_to_gem);
        position.x = static_cast<float>(padX);
        position.y = static_cast<float>(padY);
        position.z = static_cast<float>(hitCalcZ);
    }

    // -----------------------------------------------------------------------
    // Local cylinder covariance: u = R*phi, v = z, in mm².
    // Position remains global XYZ. Assume independent u/v errors and uniform
    // charge within the pad's azimuthal width. The z variance is a placeholder.
    // The third slot is unused for this 2D measurement, not a radial constraint.
    // -----------------------------------------------------------------------
    const double padWidth = getPadApproxWidth(ring);
    const double maxPadDim = std::max(padWidth, getPadHight());
    const double u_variance = get_variance(padWidth);
    const double z_variance = m_cfg_hitZVariance() * ActsUnits::mm * ActsUnits::mm;

    tdis::CovDiag3f cov{
        static_cast<float>(u_variance),
        static_cast<float>(z_variance),
        0.0F
    };

    const uint32_t cell_id = 1'000'000 * mcHit.getPlane() + 1'000 * ring + pad;

    // -----------------------------------------------------------------------
    // TrackerHit
    // -----------------------------------------------------------------------
    auto hit = m_out_trackerHits()->create();
    hit.setPosition(position);
    hit.setPositionError(cov);
    hit.setCellID(cell_id);
    hit.setTime(static_cast<float>(mcHit.getTime()));  // EDM time is in ns.
    hit.setTimeError(static_cast<float>(ActsUnits::ms / ActsUnits::ns));
    hit.setEdep(mcHit.getAdc());
    hit.setEdepError(0.0F);
    hit.setRawHit(mcHit);

    // -----------------------------------------------------------------------
    // Measurement2D
    // -----------------------------------------------------------------------
    auto actsDetElement = m_service_geometry().GetDetectorCylinder(ring);
    auto& surfaceRef    = actsDetElement->surface();
    const auto geometryId = surfaceRef.geometryId().value();

    // Tolerance for globalToLocal: use pad size when working with pad centres
    // (they lie exactly on the cylinder), or the full pad dimension when using
    // true positions that may be slightly off the surface.
    const double onSurfaceTolerance =
        shouldUseTruePos ? maxPadDim : 1.0 * Acts::UnitConstants::mm;

    Acts::Vector2 loc = Acts::Vector2::Zero();
    try {
        Acts::Vector2 pos = surfaceRef
            .globalToLocal(
                Acts::GeometryContext(),
                {position.x, position.y, position.z},
                Acts::Vector3::Zero(),
                onSurfaceTolerance)
            .value();

        loc[Acts::eBoundLoc0] = pos[0];
        loc[Acts::eBoundLoc1] = pos[1];

    } catch (const std::exception& ex) {
        m_log->warn(
            "Event #{}: globalToLocal failed for plane={} ring={} pad={} "
            "onSurfaceTolerance={:.3f}: '{}' — skipping hit",
            event_index, plane, ring, pad, onSurfaceTolerance, ex.what());
        return {std::nullopt, std::nullopt};
    }

    auto meas2D = m_out_measurements()->create();
    meas2D.setSurface(geometryId);
    meas2D.setLoc({static_cast<float>(loc[0]), static_cast<float>(loc[1])});
    meas2D.setTime(hit.getTime());
    meas2D.setCovariance({
        cov(0, 0),
        cov(1, 1),
        hit.getTimeError() * hit.getTimeError(),
    });
    meas2D.addToWeights(1.0);
    meas2D.addToHits(hit);

    return {hit, meas2D};
}

// ---------------------------------------------------------------------------
// createTrackSeedWithParameters
// ---------------------------------------------------------------------------
tdis::TrackSeed TruthTracksSeedsHitsFactory::createTrackSeedWithParameters(
    const tdis::DigitizedMtpcMcTrack&       mc_track,
    const std::vector<tdis::TrackerHit>&    trackHits,
    const std::vector<tdis::Measurement2D>& trackMeasurements)
{
    namespace ActsUnits = Acts::UnitConstants;

    // -----------------------------------------------------------------------
    // MC kinematics
    // -----------------------------------------------------------------------
    const double momentum = mc_track.getMomentum();
    const double theta    = mc_track.getTheta();
    const double phi      = mc_track.getPhi();

    // FIX Bug #3: m_cfg_momentumSmear is a dimensionless fraction (e.g. 0.1 = 10 %).
    // Previously it was incorrectly multiplied by ActsUnits::GeV (= 1000 in MeV
    // natural units), inflating the smear width by three orders of magnitude.
    const double smearedMomentum = momentum * generateNormal(1.0, m_cfg_momentumSmear());

    // -----------------------------------------------------------------------
    // Vertex position
    // FIX Bug #5: The original code unconditionally overwrote the MC vertex
    // with the first hit position, rendering the vertex configuration
    // parameter useless and producing incorrect PCA calculations.
    // Now gated behind m_cfg_useFirstHitAsVertex (default false).
    // -----------------------------------------------------------------------
    double vx = 0.0;
    double vy = 0.0;
    double vz = mc_track.getVertexZ();

    if (m_cfg_useFirstHitAsVertex()) {
        auto firstHitPos = mc_track.getHits().at(0).getTruePosition();
        vx = firstHitPos.x;
        vy = firstHitPos.y;
        vz = firstHitPos.z;
    }

    // -----------------------------------------------------------------------
    // Momentum unit vector
    // -----------------------------------------------------------------------
    const double px = std::sin(theta) * std::cos(phi);
    const double py = std::sin(theta) * std::sin(phi);
    const double pz = std::cos(theta);

    // -----------------------------------------------------------------------
    // Point of Closest Approach (PCA) to the beamline (Z-axis)
    //
    // Parametric track: r(t) = (vx + t·px, vy + t·py, vz + t·pz)
    // Distance² to Z-axis = (vx + t·px)² + (vy + t·py)²
    // Minimised when d/dt = 0  →  t = -(vx·px + vy·py) / (px² + py²)
    // -----------------------------------------------------------------------
    const double t    = -(vx * px + vy * py) / (px * px + py * py);
    const double xpca = vx + t * px;
    const double ypca = vy + t * py;
    const double zpca = vz + t * pz;

    // -----------------------------------------------------------------------
    // Perigee surface (origin at (0,0,0) on the Z-axis)
    // -----------------------------------------------------------------------
    auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(Acts::Vector3(0, 0, 0));

    const Acts::Vector3 globalPCA(xpca, ypca, zpca);
    const Acts::Vector3 direction(px, py, pz);

    auto local = perigee->globalToLocal(
        m_service_geometry->GetActsGeometryContext(),
        globalPCA,
        direction);

    if (!local.ok()) {
        m_log->error("globalToLocal failed for perigee PCA ({:.2f},{:.2f},{:.2f})",
                     xpca, ypca, zpca);
        throw std::runtime_error("Failed to create track parameters: perigee globalToLocal failed");
    }

    const Acts::Vector2 localpos = local.value();
    const double        charge   = 1.0;   // TODO: read from MC track when available

    // -----------------------------------------------------------------------
    // TrackParameters at the perigee
    // -----------------------------------------------------------------------
    auto track_param = m_out_trackParams()->create();
    track_param.setType(-1);   // seed type
    track_param.setLoc({static_cast<float>(localpos(0)), static_cast<float>(localpos(1))});
    track_param.setPhi(phi);
    track_param.setTheta(theta);
    track_param.setQOverP(charge / smearedMomentum);
    track_param.setTime(mc_track.getHits().at(0).getTime());

    // EDM covariance in [mm, mm, rad, rad, e/GeV, ns]; convert time at the ACTS boundary.
    tdis::Cov6f cov;
    cov(0, 0) = m_cfg_covLoc0()   * ActsUnits::mm * ActsUnits::mm;          // loc0  [mm²]
    cov(1, 1) = m_cfg_covLoc1()   * ActsUnits::mm * ActsUnits::mm;          // loc1  [mm²]
    cov(2, 2) = m_cfg_covPhi();                                              // phi   [rad²]
    cov(3, 3) = m_cfg_covTheta();                                            // theta [rad²]
    cov(4, 4) = m_cfg_covQOverP() / (ActsUnits::GeV * ActsUnits::GeV);      // q/p   [(e/GeV)²]
    cov(5, 5) = m_cfg_covTime();                                           // time [ns²]
    track_param.setCovariance(cov);

    // -----------------------------------------------------------------------
    // TrackSeed
    // -----------------------------------------------------------------------
    auto seed = m_out_seeds()->create();
    seed.setPerigee({
        static_cast<float>(xpca),   // PCA x  (NOT the vertex — vertex may be off-axis)
        static_cast<float>(ypca),   // PCA y
        static_cast<float>(zpca)    // PCA z
    });

    for (int i = 0; i < static_cast<int>(trackHits.size()); ++i) {
        seed.addToHits(trackHits[i]);
        if (i < static_cast<int>(trackMeasurements.size())) {
            seed.addToMeasurements(trackMeasurements[i]);
        }
    }

    seed.setInitParams(track_param);
    seed.setMcTrack(mc_track);

    m_log->trace(
        "Created seed: {} hits, vtx_z={:.2f} mm, PCA=({:.2f},{:.2f},{:.2f}) mm, "
        "p={:.3f} GeV (smeared from {:.3f} GeV)",
        trackHits.size(), vz, xpca, ypca, zpca, smearedMomentum, momentum);

    return seed;
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------
void TruthTracksSeedsHitsFactory::Execute(int32_t /*run_nr*/, uint64_t event_index) {
    namespace ActsUnits = Acts::UnitConstants;

    const auto planePositions = getPlanePositions();

    m_log->trace("TruthTracksSeedsHitsFactory: processing event {}", event_index);

    for (const auto& mc_track : *m_in_mcTracks()) {

        // Skip tracks that don't have enough MC hits to begin with
        if (mc_track.hits_size() < m_cfg_minHitsForSeed()) {
            m_log->warn("Event #{}: skipping track with {} MC hits (minimum is {})",
                        event_index, mc_track.hits_size(), m_cfg_minHitsForSeed());
            continue;
        }

        // -------------------------------------------------------------------
        // Step 1: Build TrackerHits and Measurement2Ds from MC hits
        // -------------------------------------------------------------------
        std::vector<tdis::TrackerHit>    trackHits;
        std::vector<tdis::Measurement2D> trackMeasurements;

        for (const auto& mcHit : mc_track.getHits()) {
            auto [hit_opt, meas_opt] =
                createHitAndMeasurement(mcHit, event_index, planePositions);

            if (hit_opt.has_value() && meas_opt.has_value()) {
                trackHits.push_back(hit_opt.value());
                trackMeasurements.push_back(meas_opt.value());
            }
        }

        // Skip if too few valid hits survived the digitisation / geometry checks
        if (static_cast<int>(trackHits.size()) < m_cfg_minHitsForSeed()) {
            m_log->debug(
                "Event #{}: skipping track — only {}/{} hits survived processing",
                event_index, trackHits.size(), mc_track.hits_size());
            continue;
        }

        // -------------------------------------------------------------------
        // Step 2: Create TrackSeed with initialised TrackParameters
        // -------------------------------------------------------------------
        try {
            createTrackSeedWithParameters(mc_track, trackHits, trackMeasurements);
        } catch (const std::exception& e) {
            m_log->error("Event #{}: failed to create track seed: {}", event_index, e.what());
            continue;
        }
    }

    m_log->debug("Event #{}: created {} seeds from MC tracks",
                 event_index, m_out_seeds()->size());
}

} // namespace tdis::tracking
