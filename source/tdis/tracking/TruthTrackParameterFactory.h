#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <JANA/JFactory.h>
#include <bits/random.h>
#include <Acts/Surfaces/PerigeeSurface.hpp>
#include <random>

#include "ActsGeometryService.h"
#include "PadGeometryHelper.hpp"
#include "podio_model/DigitizedMtpcMcHit.h"
#include "podio_model/DigitizedMtpcMcTrack.h"
#include "podio_model/MutableTrackerHit.h"
#include "podio_model/TrackParametersCollection.h"
#include "podio_model/TrackerHit.h"
#include "podio_model/TrackerHitCollection.h"


namespace tdis::tracking {

    struct TruthTrackParameterFactory : public JOmniFactory<TruthTrackParameterFactory> {
        PodioInput<tdis::DigitizedMtpcMcTrack> m_mc_tracks_in {this, {"DigitizedMtpcMcTrack"}};
        PodioOutput<tdis::TrackParameters> m_trackers_out {this, "TruthTrackInitParameters"};
        Service<ActsGeometryService> m_service_geometry{this};
        Service<services::LogService> m_service_log{this};

        Parameter<bool> m_cfg_use_true_pos{this, "acts:use_true_position", true,"Use true hits xyz instead of digitized one"};
        Parameter<double> m_cfg_momentum_smear{this, "acts:track_init:momentum_smear", 0.1,"GeV, Momentum smear for truth track initialization"};
        std::shared_ptr<spdlog::logger> m_log;


        void Configure() {
            m_service_geometry();
            m_log = m_service_log->logger("tracking:hit_reco");
        }

        void ChangeRun(int32_t /*run_nr*/) {
        }

        // Function to generate the next value in a normal distribution
        double generateNormal(double mean, double stddev) {
            // Create a random device and a generator
            static std::random_device rd;
            static std::mt19937 generator(rd());

            // Create a normal distribution with the given mean and standard deviation
            std::normal_distribution<double> distribution(mean, stddev);

            // Generate and return the next value
            return distribution(generator);
        }

        void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {
            using namespace Acts::UnitLiterals;


            auto track_parameters = std::make_unique<tdis::TrackParametersCollection>();
            auto plane_positions = m_service_geometry->GetPlanePositions();


            // Loop over input particles
            for (const auto& mc_track: *m_mc_tracks_in()) {

                // We need at least one hit
                if(!mc_track.hits_size()) {
                    continue;
                }

                // require close to interaction vertex
                auto v = mc_track.getHits().at(0).getTruePosition();  // Use it as a vertex for now
                double vx = v.x;
                double vy = v.y;
                double vz = v.z;


                double magnitude = mc_track.getMomentum();
                double theta = mc_track.getTheta();
                double phi = mc_track.getPhi();
                const auto eta   = -std::log(std::tan(theta/2));
                double px = magnitude * std::sin(theta) * std::cos(phi);
                double py = magnitude * std::sin(theta) * std::sin(phi);
                double pz = magnitude * std::cos(theta);

                // require minimum momentum
                const auto& p = mc_track.getMomentum();
                const auto pmag = std::hypot(px, py, pz);

                // modify initial momentum to avoid bleeding truth to results when fit fails
                const auto pinit = pmag * generateNormal(1, m_cfg_momentum_smear()*Acts::UnitConstants::GeV);

                // define line surface for local position values
                auto perigee = Acts::Surface::makeShared<Acts::PerigeeSurface>(Acts::Vector3(0,0,0));

                // track particle back to transverse point-of-closest approach
                // with respect to the defined line surface
                auto linesurface_parameter = -(vx*px + vy*py)/(px*px + py*py);

                auto xpca = v.x + linesurface_parameter*px;
                auto ypca = v.y + linesurface_parameter*py;
                auto zpca = v.z + linesurface_parameter*pz;

                Acts::Vector3 global(xpca, ypca, zpca);
                Acts::Vector3 direction(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta));

                // convert from global to local coordinates using the defined line surface
                auto local = perigee->globalToLocal(m_service_geometry->GetActsGeometryContext(), global, direction);

                if(!local.ok())
                {
                    m_log->error("skipping the track because globaltoLocal function failed");
                    continue;
                }

                Acts::Vector2 localpos = local.value();
                double charge = 1;  // TODO ??? Proton?

                // Insert into tdis::TrackParameters, which uses numerical values in its specified units
                auto track_parameter = track_parameters->create();
                track_parameter.setType(-1); // type --> seed(-1)
                track_parameter.setLoc({static_cast<float>(localpos(0)), static_cast<float>(localpos(1))}); // 2d location on surface [mm]
                track_parameter.setPhi(phi); // phi [rad]
                track_parameter.setTheta(theta); // theta [rad]
                track_parameter.setQOverP(charge / (pinit)); // Q/p [e/GeV]
                track_parameter.setTime(mc_track.getHits().at(0).getTime()); // time [ns]
                tdis::Cov6f cov;
                cov(0,0) = 1.0; // loc0
                cov(1,1) = 1.0; // loc1
                cov(2,2) = 0.05; // phi
                cov(3,3) = 0.01; // theta
                cov(4,4) = 0.1; // qOverP
                cov(5,5) = 10e9; // time
                track_parameter.setCovariance(cov);
            }



            m_trackers_out() = std::move(track_parameters);
        }
    };
}