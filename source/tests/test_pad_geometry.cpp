#include "geometry/TdisGeometryHelper.hpp"

#include <iostream>

int main() {
    const double pitch = 2 * M_PI / num_pads_per_ring;
    for (int ring = 0; ring < num_rings; ++ring) {
        const double radius = tdis::getRingCenterRadius(ring);
        for (int pad = 0; pad < num_pads_per_ring; ++pad) {
            const auto [x, y] = tdis::getPadCenter(ring, pad);
            const double expected = (pad + (ring % 2) * .5) * pitch;
            if (std::hypot(x - radius * std::cos(expected), y - radius * std::sin(expected)) > 1e-10) {
                throw std::runtime_error("Pad centre convention is incorrect");
            }
            const auto [rotatedX, rotatedY] = tdis::getPadCenter(ring, (pad + 17) % num_pads_per_ring);
            const double angle = 17 * pitch;
            if (std::hypot(rotatedX - (x * std::cos(angle) - y * std::sin(angle)),
                           rotatedY - (x * std::sin(angle) + y * std::cos(angle))) > 1e-10) {
                throw std::runtime_error("Pad geometry breaks rotational symmetry");
            }
        }
    }
    std::cout << "PASS: pad centre indices, odd-ring stagger and pad-pitch rotational symmetry\n";
}
