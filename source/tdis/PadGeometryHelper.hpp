#pragma once

#include <cmath>       // For std::cos, std::sin, M_PI
#include <stdexcept>   // For std::invalid_argument
#include <utility>     // For std::pair
#include <Acts/Definitions/Units.hpp>

// Constants
constexpr int num_rings = 21;
constexpr int num_pads_per_ring = 122;
constexpr double min_radius = 5.0 * Acts::UnitConstants::cm;         // Minimum radius in cm
constexpr double max_radius = 15.0 * Acts::UnitConstants::cm;        // Maximum radius in cm
constexpr double total_radial_width = max_radius - min_radius;       // Total radial width (10 cm)
constexpr double ring_width = total_radial_width / num_rings;        // Radial width of each ring
constexpr double delta_theta = 2 * M_PI / num_pads_per_ring;         // Angular width of each pad in radians

namespace tdis {

    inline double getPadHight() {
        /** gets pad height which is the distance between rings*/
        return (max_radius - min_radius) / num_pads_per_ring;
    }

    inline double getPadApproxWidth(const int ring) {
        /** gets pad approximate width calculated from curvature, which is not exact the width */
        const double r = min_radius + (ring + 0.5) * getPadHight();
        return r * 2 * M_PI / num_pads_per_ring;
    }

    inline double getRingCenterRadius(const int ring) {
        /*
        Compute the X and Y coordinates of the center of a pad given its ring and pad indices.

        Parameters:
        - ring (int): The ring index (0 is the innermost ring).
        - pad (int): The pad index (0 is at or closest to φ=0, numbering is clockwise).

        Returns:
        - std::pair<double, double>: X and Y coordinates of the pad center in Acts::UnitConstants.
        */

        // Validate inputs
        if (ring < 0 || ring >= num_rings) {
            throw std::invalid_argument("Ring index must be between 0 and " + std::to_string(num_rings - 1));
        }

        // Compute radial center of the ring
        const double r_center = min_radius + ring_width * (ring + 0.5);
        return r_center;
    }

    inline std::tuple<double, double> getPadCenter(const int ring, const int pad) {
        /*
        Compute the X and Y coordinates of the center of a pad given its ring and pad indices.

        Parameters:
        - ring (int): The ring index (0 is the innermost ring).
        - pad (int): The pad index (0 is at or closest to φ=0, numbering is clockwise).

        Returns:
        - std::pair<double, double>: X and Y coordinates of the pad center in Acts::UnitConstants.
        */

        // Validate inputs
        if (ring < 0 || ring >= num_rings) {
            throw std::invalid_argument("Ring index must be between 0 and " + std::to_string(num_rings - 1));
        }
        if (pad < 0 || pad >= num_pads_per_ring) {
            throw std::invalid_argument("Pad index must be between 0 and " + std::to_string(num_pads_per_ring - 1));
        }

        // Compute radial center of the ring
        const double r_center = getRingCenterRadius(ring);

        // Determine the angular offset for odd rings
        const double theta_offset = (ring % 2 == 0) ? 0.0 : delta_theta / 2.0;

        // Compute the angular center of the pad (in radians)
        const double theta_clockwise = pad * delta_theta + theta_offset + delta_theta / 2.0;

        // Convert to counterclockwise angle for standard coordinate system
        //double theta_center = 2 * M_PI - theta_clockwise;
        double theta_center = theta_clockwise;

        // Ensure theta_center is within [0, 2π]
        if (theta_center < 0) {
            theta_center += 2 * M_PI;
        }

        // Convert from polar to Cartesian coordinates
        double x = r_center * std::cos(theta_center);
        double y = r_center * std::sin(theta_center);

        return { x, y };
    }

}