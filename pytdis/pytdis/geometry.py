"""
Geometry functions for TDIS mTPC detector.

This module provides functions to calculate positions and geometry
of the detector components, particularly the pad arrangements.


The mTPC will consist of 10 separate TPC volumes which will be assembled together to form the mTPC. The entire
detector will be 55 cm long with an annulus of inner radius of 5 cm and an outer radius of 15 cm.

```
[ - sensitive plane positive in Z direction
| - cathode wall
] - sensitive plane negative in Z direction

mtpc diagram:
[   ||   ][   ||   ][   ||   ][   ||   ][   ||   ]
--------------------------------------------------> z axis

```

The pads are arranged into 21 concentric rings, each with a radial with of 10 cm/21.
The rings are numbered from 0 to 20 sequentially, with 0 being the innermost ring.
Each ring has 122 pads, yielding a total of 2 562 pads per readout plane
and a total of 25 620 readout pads if all 10 chambers of the mTPC are instrumented.
The φ angle of each pad is as defined in the Lab coordinate system.
For even rings the centre of pad 0 is located at φ = 0◦.
Whereas for odd rings the center is shifted by a half of pad angle, i.e. is at φ = 0.5×2π/122 =∼ 0.02575◦.
Since the ring widths and number of pads per ring are fixed,
the pads are rectangular with a different aspect ratio for each ring, depending on the radius,
i.e. smaller pads are located at the innermost radii.
"""
import math

import numpy as np


# Detector constants
NUM_RINGS = 21
NUM_PADS_PER_RING = 122
FIRST_RING_INNER_RADIUS = 5  # cm
PLANE_RING_RADIUS = 10  # Total radius span in cm
LAST_RING_OUTER_RADIUS = FIRST_RING_INNER_RADIUS + PLANE_RING_RADIUS
RING_WIDTH = PLANE_RING_RADIUS / NUM_RINGS  # Radial width of each ring in cm

# Angular width of each pad
DELTA_THETA_DEG = 360.0 / NUM_PADS_PER_RING
DELTA_THETA = 2 * np.pi / NUM_PADS_PER_RING


def get_pad_center(ring: int, pad: int) -> tuple[float, float]:
    """
    Compute the X and Y coordinates of the center of a pad given its ring and pad indices.

    Parameters
    ----------
    ring : int
        The ring index (0 is the innermost ring).
    pad : int
        The pad index (0 is at or closest to φ=0°, numbering is clockwise).

    Returns
    -------
    x : float
        X-coordinate of the pad center in cm.
    y : float
        Y-coordinate of the pad center in cm.

    Raises
    ------
    ValueError
        If ring or pad indices are out of valid range.
    """
    # Validate inputs
    if not (0 <= ring < NUM_RINGS):
        raise ValueError(f"Ring index must be between 0 and {NUM_RINGS - 1}")
    if not (0 <= pad < NUM_PADS_PER_RING):
        raise ValueError(f"Pad index must be between 0 and {NUM_PADS_PER_RING - 1}")

    # Compute radial center of the ring
    r_center = RING_WIDTH * (ring + 0.5) + FIRST_RING_INNER_RADIUS

    # Determine the angular offset for odd rings
    if (ring) % 2 == 0:
        theta_offset = 0  # Even rings
    else:
        theta_offset = DELTA_THETA / 2  # Odd rings

    # Compute the angular center of the pad
    theta_center =  pad * DELTA_THETA + theta_offset + DELTA_THETA

    # Convert from polar to Cartesian coordinates
    x = r_center * np.cos(theta_center)
    y = r_center * np.sin(theta_center)

    return x, y


def get_ring_radii(ring: int) -> tuple[float, float]:
    """
    Get the inner and outer radii for a given ring.

    Parameters
    ----------
    ring : int
        The ring index (0 is the innermost ring).

    Returns
    -------
    r_inner : float
        Inner radius of the ring in cm.
    r_outer : float
        Outer radius of the ring in cm.
    """
    if not (0 <= ring < NUM_RINGS):
        raise ValueError(f"Ring index must be between 0 and {NUM_RINGS - 1}")

    r_inner = FIRST_RING_INNER_RADIUS + ring * RING_WIDTH
    r_outer = FIRST_RING_INNER_RADIUS + (ring + 1) * RING_WIDTH

    return r_inner, r_outer


def get_pad_angular_bounds(ring: int, pad: int) -> tuple[float, float]:
    """
    Get the angular bounds (in radians) for a given pad.

    Parameters
    ----------
    ring : int
        The ring index (0 is the innermost ring).
    pad : int
        The pad index (0 is at or closest to φ=0°).

    Returns
    -------
    theta_start : float
        Starting angle in radians.
    theta_end : float
        Ending angle in radians.
    """
    if not (0 <= ring < NUM_RINGS):
        raise ValueError(f"Ring index must be between 0 and {NUM_RINGS - 1}")
    if not (0 <= pad < NUM_PADS_PER_RING):
        raise ValueError(f"Pad index must be between 0 and {NUM_PADS_PER_RING - 1}")

    # Determine the angular offset for odd rings
    if ring % 2 == 0:
        theta_offset = 0
    else:
        theta_offset = DELTA_THETA / 2

    theta_start = pad * DELTA_THETA + theta_offset
    theta_end = theta_start + DELTA_THETA

    return theta_start, theta_end

def get_pad_approx_width(ring):
    """gets pad approximate width calculated from curvature, which is not exact the width"""
    r = FIRST_RING_INNER_RADIUS + (ring + 0.5) * RING_WIDTH
    return r * 2 * math.pi / NUM_PADS_PER_RING