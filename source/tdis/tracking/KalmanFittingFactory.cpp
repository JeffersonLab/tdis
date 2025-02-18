#include "KalmanFittingFactory.h"

// ACTS
#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Definitions/TrackParametrization.hpp>
#include <Acts/Definitions/Units.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/MagneticField/ConstantBField.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/Propagator/EigenStepper.hpp>
#include <Acts/Propagator/PropagatorPlainOptions.hpp>
#include <Acts/TrackFitting/GainMatrixUpdater.hpp>
#include <Acts/TrackFitting/GainMatrixSmoother.hpp>
#include <Acts/TrackFitting/KalmanFitter.hpp>
#include <Acts/Utilities/Logger.hpp>
#include <Acts/Utilities/Helpers.hpp>

// For track containers & SourceLinks
#include <Acts/EventData/SourceLink.hpp>
#include <Acts/EventData/MultiTrajectory.hpp>
#include <Acts/EventData/TrackParameters.hpp>
#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>

// Example "IndexSourceLink" that wraps measurement index
#include <ActsExamples/EventData/IndexSourceLink.hpp>
#include <ActsExamples/EventData/Track.hpp> // for ActsExamples::ConstTrackContainer

// Our EDM
#include "podio_model/DigitizedMtpcMcTrackCollection.h"
#include "podio_model/Measurement2DCollection.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/TrackerHitCollection.h"
#include "podio_model/DigitizedMtpcMcHit.h"

#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace tdis {
namespace tracking {

void KalmanFittingFactory::Configure() {
    // Acquire logger
    m_log = m_service_log->logger("KalmanFitting");

    // Just log some info
    m_log->info("KalmanFittingFactory configured");
}

void KalmanFittingFactory::ChangeRun(int32_t /*runNumber*/) {
    // Nothing special needed
}

/// Dummy calibrator for demonstration (no cluster calibration)
struct NullMeasurementCalibrator : public Acts::ICalibrationContext {
    // If your code needs residual scaling or drift corrections,
    // you would implement them. We do nothing here.
};

void KalmanFittingFactory::Execute(int32_t /*runNumber*/, uint64_t eventNumber) {

    m_log->debug("KalmanFittingFactory::Execute() begin event={} ...", eventNumber);

<<<<<<< Updated upstream
    // Construct a perigee surface as the target surface
    auto pSurface = Acts::Surface::makeShared<Acts::PerigeeSurface>(
        Acts::Vector3{0., 0., 0.});

    // // Type erased calibrator for the measurements
    // std::shared_ptr<ActsExamples::MeasurementCalibrator> calibrator;
    //
    // double bz = 1.5 * Acts::UnitConstants::T;
    //
    // auto field = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0.0, 0.0, bz));
    //
    //
    // // Construct the MagneticFieldContext using the cache
    // Acts::MagneticFieldContext magFieldContext;
    // // Create a cache for the magnetic field
    // //Acts::ConstantBField::Cache magFieldCache = field->makeCache(magFieldContext);
    // Acts::CalibrationContext calibrationContext;
    //
    // ActsExamples::TrackFitterFunction::GeneralFitterOptions options{
    //     m_serviceGeometry->GetActsGeometryContext(),
    //     magFieldContext, calibrationContext, pSurface.get(),
    //     Acts::PropagatorPlainOptions(m_serviceGeometry->GetActsGeometryContext(), magFieldContext)};
    //
    // auto trackContainer = std::make_shared<Acts::VectorTrackContainer>();
    // auto trackStateContainer = std::make_shared<Acts::VectorMultiTrajectory>();
    // Acts::TrackContainer tracks(trackContainer, trackStateContainer);
    //
    // // Perform the fit for each input track
    // std::vector<Acts::SourceLink> trackSourceLinks;
    //
    // for (auto track: *m_parameters_input()) {
    //     track.
    //
    // }
=======
    // 1) Grab geometry context from ActsGeometryService
    auto geoCtx = m_service_geo->GetActsGeometryContext();

    // 2) Prepare a B-Field (example: 1.5T uniform along z)
    double bz = 1.5 * Acts::UnitConstants::T;
    auto bField = std::make_shared<Acts::ConstantBField>(Acts::Vector3(0, 0, bz));
    Acts::MagneticFieldContext magFieldContext; // empty payload, for constant field

    // 3) Construct plain-propagator options
    Acts::PropagatorPlainOptions plainOpts(geoCtx, magFieldContext);

    // 4) Setup the Gains matrix-based KalmanFitter
    using Updater  = Acts::GainMatrixUpdater;
    using Smoother = Acts::GainMatrixSmoother;
    using TrackStateBackend     = Acts::MultiTrajectory;
    using TrackStateArrayCreator= Acts::TrackStateArrayCreator;

    using Fitter = Acts::KalmanFitter<Updater, Smoother, TrackStateBackend, TrackStateArrayCreator>;

    // The stepper & navigator for the Fitter::Config
    using Stepper = Acts::EigenStepper<>;
    Stepper stepper(bField);
    Acts::Navigator::Config navCfg;
    // we can fetch the geometry from the service as well:
    navCfg.trackingGeometry = m_service_geo->GetTrackingGeometry();
    navCfg.resolvePassive   = false; // optional
    navCfg.resolveMaterial  = true;
    navCfg.resolveSensitive = true;
    Acts::Navigator navigator(navCfg);

    // Build the Propagator
    auto propagator = std::make_shared<Acts::Propagator<Stepper, Acts::Navigator>>(
        std::move(stepper), std::move(navigator)
    );

    // Build the actual KalmanFitter
    Fitter::Config kfCfg(std::move(propagator));
    Fitter kalmanFitter(std::move(kfCfg));

    // "Calibrator" if needed - we create a trivial placeholder
    NullMeasurementCalibrator dummyCalibrator;

    // 5) We want to read the TDIS MC tracks and the 2D measurements
    auto& mc_tracks = *m_mc_tracks_in();       // DigitizedMtpcMcTrack collection
    auto& meas2d    = *m_measurements_in();    // Measurement2D collection

    m_log->debug("Found {} MC tracks, {} 2D measurements", mc_tracks.size(), meas2d.size());
    if (mc_tracks.empty() || meas2d.empty()) {
        // Return empty output
        m_acts_tracks_out() = std::make_shared<ActsExamples::ConstTrackContainer>(
            nullptr, nullptr
        );
        return;
    }
>>>>>>> Stashed changes

    // 6) Pre-build a map from each rawHit (DigitizedMtpcMcHit) to measurement indices.
    //    Each Measurement2D can have multiple TrackerHits, each with rawHit => DigitizedMtpcMcHit
    //    We store them so we can look up quickly which measurement(s) correspond to a given mcHit.
    //    Because OneToMany is possible, we store vectors of measurement indices.
    std::unordered_map<podio::ObjectID, std::vector<size_t>> hitToMeasIndex;

    for (size_t im=0; im<meas2d.size(); ++im) {
        const auto& m2d = meas2d.at(im);
        // each Measurement2D has a OneToMany relation: hits[]
        auto hitsRefs = m2d.hits();
        for (auto hRef : hitsRefs) {
            if (!hRef.isAvailable()) continue;
            // Each TrackerHit might have rawHit => DigitizedMtpcMcHit
            auto rawRef = hRef.rawHit();
            if (!rawRef.isAvailable()) continue;

            // Insert the measurement index into the map for this rawHit ID
            auto rawID = rawRef.getObjectID();
            hitToMeasIndex[rawID].push_back(im);
        }
    }

    // 7) Prepare containers for final tracks
    auto trackContainer      = std::make_shared<Acts::VectorTrackContainer>();
    auto trackStateContainer = std::make_shared<Acts::VectorMultiTrajectory>();
    Acts::TrackContainer actsTracks(trackContainer, trackStateContainer);

    // 8) Fit each MC track from DigitizedMtpcMcTrack
    for (size_t iTrk=0; iTrk<mc_tracks.size(); ++iTrk) {
        auto mc_trk = mc_tracks.at(iTrk);

        // -- 8a) Convert MC track info into an approximate Acts::BoundTrackParameters
        //        We treat (0,0, mc_trk.vertexZ) as the initial position (in mm).
        //        Convert phi/theta from degrees to radians.
        double phiRad   = mc_trk.phi()   * M_PI/180.0;
        double thetaRad = mc_trk.theta() * M_PI/180.0;
        double p        = mc_trk.momentum(); // [GeV/c]
        double q        = 1.0; // Hard-coded +1 e charge? Or glean from PDG?

        // If you know the particle is negative, set q = -1.
        // If there's more info in your data model, use it.

        // Position in mm (DigitizedMtpcMcTrack stores vertexZ in meters => convert)
        double z_mm = mc_trk.vertexZ() * 1000.0;
        Acts::Vector3 startPos(0., 0., z_mm);

        // Momentum in GeV, convert direction:
        double px = p * std::sin(thetaRad) * std::cos(phiRad);
        double py = p * std::sin(thetaRad) * std::sin(phiRad);
        double pz = p * std::cos(thetaRad);
        Acts::Vector3 startMom(px, py, pz);

        // Bound covariance: minimal guess => identity
        Acts::BoundSymMatrix cov = Acts::BoundSymMatrix::Zero();
        // seed some uncertainties, e.g. 1 mm on position, ~0.1 rad on angles, ...
        cov(Acts::eBoundLoc0, Acts::eBoundLoc0) = 1.0 * Acts::UnitConstants::mm2; // x'ish
        cov(Acts::eBoundLoc1, Acts::eBoundLoc1) = 1.0 * Acts::UnitConstants::mm2; // y'ish
        cov(Acts::eBoundPhi,  Acts::eBoundPhi)  = 0.01; // rad^2
        cov(Acts::eBoundTheta,Acts::eBoundTheta)= 0.01; // rad^2
        cov(Acts::eBoundQOverP, Acts::eBoundQOverP)= (1.0/(p*p)) * 0.1; // naive

        // Make curvilinear parameters from (pos, mom, q, time=0, cov)
        auto initialParams = Acts::makeCurvilinearParameters(
            geoCtx, startPos, startMom, q, 0.0, cov
        );

        m_log->trace("Track #{} truth init: p={}GeV, phi={}deg, theta={}deg, vertexZ={}m => #hits={}",
                     iTrk, p, mc_trk.phi(), mc_trk.theta(), mc_trk.vertexZ(), mc_trk.hits_size());

        // -- 8b) Build the SourceLink list from the hits that belong to this MC track
        std::vector<Acts::SourceLink> trackSourceLinks;
        trackSourceLinks.reserve(mc_trk.hits_size());

        // Each DigitizedMtpcMcTrack has a OneToMany of DigitizedMtpcMcHit
        auto hitsRefs = mc_trk.hits();
        for (auto hRef : hitsRefs) {
            if (!hRef.isAvailable()) continue;
            // Which measurement(s) use this rawHit?
            auto hID = hRef.getObjectID();
            auto it = hitToMeasIndex.find(hID);
            if (it == hitToMeasIndex.end()) continue;

            // The same MC hit might appear in multiple measurement2D if your
            // design merges hits? Usually it’s 1-to-1, but we handle the vector anyway.
            for (auto measIndex : it->second) {
                // ActsExamples uses IndexSourceLink to store an integer index
                // to the measurement container. geometryId = from the measurement
                // itself.
                const auto& thisMeasurement = meas2d.at(measIndex);
                Acts::GeometryIdentifier geoId(thisMeasurement.surface());
                // Build a "IndexSourceLink"
                ActsExamples::IndexSourceLink sLink(geoId, measIndex);
                trackSourceLinks.push_back(Acts::SourceLink(sLink));
            }
        }

        if (trackSourceLinks.empty()) {
            // No hits => skip
            m_log->warn("Track #{} has zero matched measurements => skip fitting.", iTrk);
            continue;
        }

        // -- 8c) Perform the fit
        // Prepare context objects
        Acts::KalmanFitterOptions<Acts::PropagatorPlainOptions, NullMeasurementCalibrator>
            kfOptions(geoCtx, magFieldContext, &dummyCalibrator, plainOpts);

        // run the fit
        auto fitResult = kalmanFitter.fit(trackSourceLinks, initialParams, kfOptions, actsTracks);

        if (fitResult.ok()) {
            const auto& fittedTrack = fitResult.value();
            if (fittedTrack.hasParameters()) {
                auto pars = fittedTrack.parameters();
                m_log->info("Kalman fit succeeded for track #{}. Fitted pars: (loc0={}, loc1={}, phi={}, theta={}, q/p={})",
                            iTrk,
                            pars[Acts::eBoundLoc0],
                            pars[Acts::eBoundLoc1],
                            pars[Acts::eBoundPhi],
                            pars[Acts::eBoundTheta],
                            pars[Acts::eBoundQOverP] );
            } else {
                m_log->warn("Fit result has no fitted parameters for track #{}", iTrk);
            }
        }
        else {
            m_log->warn("Kalman fit **failed** for track #{}. Error code: {}: {}",
                        iTrk, fitResult.error(), fitResult.error().message());
        }
    }

    // 9) Create a “ConstTrackContainer” from these vector containers
    //    Acts::TrackContainer => make it const
    auto constTrackContainer = std::make_shared<Acts::ConstVectorTrackContainer>(
        std::move(*trackContainer));
    auto constMultiTrajectory = std::make_shared<Acts::ConstVectorMultiTrajectory>(
        std::move(*trackStateContainer));
    ActsExamples::ConstTrackContainer constTracks(constTrackContainer, constMultiTrajectory);

    // 10) Put final container in JANA's output
    m_acts_tracks_out() = constTracks;

    m_log->debug("KalmanFittingFactory::Execute() done event={}", eventNumber);
}

} // end namespace tracking
} // end namespace tdis
