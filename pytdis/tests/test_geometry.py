"""Unit tests for pytdis.geometry module."""

import unittest
import numpy as np
from pytdis.geometry import (
    get_pad_center,
    get_ring_radii,
    get_pad_angular_bounds,
    NUM_RINGS,
    NUM_PADS_PER_RING,
    FIRST_RING_INNER_RADIUS,
    LAST_RING_OUTER_RADIUS,
    RING_WIDTH,
    DELTA_THETA, get_pad_approx_width
)


class TestGeometryFunctions(unittest.TestCase):
    """Test cases for geometry functions."""

    def test_get_pad_center_valid(self):
        """Test get_pad_center with valid inputs."""
        # Test innermost ring, pad 0
        x, y = get_pad_center(0, 0)
        # For ring 0, pad 0 should be near positive x-axis
        self.assertGreater(x, 0)
        self.assertAlmostEqual(y, 0, places=1)

        # Test that radius is correct
        r = np.sqrt(x**2 + y**2)
        expected_r = FIRST_RING_INNER_RADIUS + 0.5 * RING_WIDTH
        self.assertAlmostEqual(r, expected_r, places=5)

    def test_get_pad_center_valid(self):
        """Take data from MC and see true values vs calculated values"""

        # 985.265	2.15224e-08	0.0309282	-0.0778114	0.118387	7	97	7	0.0316129
        # Test innermost ring, pad 0
        x, y = get_pad_center(7, 97)

        # delta_x =
        #
        self.assertAlmostEqual(x, 3.09, delta=RING_WIDTH)
        self.assertAlmostEqual(y, 7.78, delta=RING_WIDTH)

        print(x,y, get_pad_approx_width(7), RING_WIDTH)



    def test_get_pad_center_odd_ring_offset(self):
        """Test that odd rings have angular offset."""

        # Even ring
        x0, y0 = get_pad_center(0, 0)
        angle0 = np.arctan2(y0, x0)

        # Odd ring
        x1, y1 = get_pad_center(1, 0)
        angle1 = np.arctan2(y1, x1)

        # Odd rings should have offset of DELTA_THETA/2
        expected_offset = DELTA_THETA / 2
        actual_offset = angle1 - angle0
        self.assertAlmostEqual(actual_offset, expected_offset, places=5)

    def test_get_pad_center_invalid_ring(self):
        """Test get_pad_center with invalid ring index."""
        with self.assertRaises(ValueError):
            get_pad_center(-1, 0)

        with self.assertRaises(ValueError):
            get_pad_center(NUM_RINGS, 0)

    def test_get_pad_center_invalid_pad(self):
        """Test get_pad_center with invalid pad index."""
        with self.assertRaises(ValueError):
            get_pad_center(0, -1)

        with self.assertRaises(ValueError):
            get_pad_center(0, NUM_PADS_PER_RING)

    def test_get_ring_radii(self):
        """Test get_ring_radii function."""
        # Test innermost ring
        r_inner, r_outer = get_ring_radii(0)
        self.assertAlmostEqual(r_inner, FIRST_RING_INNER_RADIUS, places=5)
        self.assertAlmostEqual(r_outer, FIRST_RING_INNER_RADIUS + RING_WIDTH, places=5)

        # Test outermost ring
        r_inner, r_outer = get_ring_radii(NUM_RINGS - 1)
        self.assertAlmostEqual(r_outer, LAST_RING_OUTER_RADIUS, places=5)

        # Test that rings are contiguous
        for ring in range(NUM_RINGS - 1):
            _, r_outer_current = get_ring_radii(ring)
            r_inner_next, _ = get_ring_radii(ring + 1)
            self.assertAlmostEqual(r_outer_current, r_inner_next, places=5)

    def test_get_pad_angular_bounds(self):
        """Test get_pad_angular_bounds function."""
        # Test that pads cover full circle
        total_angle = 0
        for pad in range(NUM_PADS_PER_RING):
            theta_start, theta_end = get_pad_angular_bounds(0, pad)
            total_angle += (theta_end - theta_start)
        self.assertAlmostEqual(total_angle, 2 * np.pi, places=5)

        # Test odd ring offset
        theta_start_even, _ = get_pad_angular_bounds(0, 0)
        theta_start_odd, _ = get_pad_angular_bounds(1, 0)
        expected_offset = DELTA_THETA / 2
        actual_offset = theta_start_odd - theta_start_even
        self.assertAlmostEqual(actual_offset, expected_offset, places=5)

    def test_consistency_pad_center_and_bounds(self):
        """Test consistency between pad center and angular bounds."""
        for ring in [0, 1, 10, 20]:  # Test a few rings
            for pad in [0, 61, 121]:  # Test a few pads
                # Get pad center
                x, y = get_pad_center(ring, pad)
                center_angle = np.arctan2(y, x)
                if center_angle < 0:
                    center_angle += 2 * np.pi

                # Get angular bounds
                theta_start, theta_end = get_pad_angular_bounds(ring, pad)

                # Center angle should be between bounds
                # Handle wrap-around at 2π
                if theta_start < theta_end:
                    self.assertGreaterEqual(center_angle, theta_start - 0.1)
                    self.assertLessEqual(center_angle, theta_end + 0.1)
                else:  # Wrap-around case
                    self.assertTrue(
                        center_angle >= theta_start - 0.1 or
                        center_angle <= theta_end + 0.1
                    )
