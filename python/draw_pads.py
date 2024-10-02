import matplotlib.pyplot as plt
from matplotlib.patches import Wedge
import numpy as np

# Set up the figure and axis
fig, ax = plt.subplots(figsize=(8, 8))
ax.set_aspect('equal')

# Constants
num_rings = 21
num_pads_per_ring = 122

first_ring_inner_radius = 5
plane_ring_radius = 10  # Total radius in cm
last_ring_outer_radius = first_ring_inner_radius + plane_ring_radius
ring_width = plane_ring_radius / num_rings  # Radial width of each ring in cm

# Angular width of each pad in degrees
delta_theta_deg = 360.0 / num_pads_per_ring

# Angular width of each pad in radians
delta_theta = 2 * np.pi / num_pads_per_ring


def get_pad_center(ring, pad):
    """
    Compute the X and Y coordinates of the center of a pad given its ring and pad indices.

    Parameters:
    - ring (int): The ring index (0 is the innermost ring).
    - pad (int): The pad index (0 is at or closest to φ=0°, numbering is clockwise).

    Returns:
    - x (float): X-coordinate of the pad center in cm.
    - y (float): Y-coordinate of the pad center in cm.
    """

    # Validate inputs
    if not (0 <= ring < num_rings):
        raise ValueError(f"Ring index must be between 0 and {num_rings - 1}")
    if not (0 <= pad < num_pads_per_ring):
        raise ValueError(f"Pad index must be between 0 and {num_pads_per_ring - 1}")

    # Compute radial center of the ring
    r_center = ring_width * (ring + 0.5) + first_ring_inner_radius

    # Determine the angular offset for odd rings
    if ring % 2 == 0:
        theta_offset = 0  # Even rings
    else:
        theta_offset = delta_theta / 2  # Odd rings

    # Compute the angular center of the pad
    theta_center = delta_theta/2 + pad * delta_theta + theta_offset

    # Convert from polar to Cartesian coordinates
    x = r_center * np.cos(theta_center)
    y = r_center * np.sin(theta_center)

    return x, y

# Loop over each ring
for r in range(num_rings):
    r_inner = first_ring_inner_radius + r * ring_width
    r_outer = first_ring_inner_radius + (r + 1) * ring_width

    # Determine the angular offset for odd rings
    if r % 2 == 0:
        theta_offset = 0
    else:
        theta_offset = delta_theta_deg / 2

    # Loop over each pad in the ring
    for p in range(num_pads_per_ring):
        theta_start = p * delta_theta_deg + theta_offset
        theta_end = theta_start + delta_theta_deg

        # Create a wedge representing the pad
        wedge = Wedge(center=(0, 0),
                      r=r_outer,
                      theta1=theta_start,
                      theta2=theta_end,
                      width=ring_width,
                      facecolor='lightblue',
                      edgecolor='black',
                      linewidth=0.5)

        ax.add_patch(wedge)

# Set plot limits and labels
ax.set_xlim(-last_ring_outer_radius, last_ring_outer_radius)
ax.set_ylim(-last_ring_outer_radius, last_ring_outer_radius)
ax.set_xlabel('X [cm]')
ax.set_ylabel('Y [cm]')
ax.set_title('Disk with Pads Arrangement')

test_ring_pads_pairs = [
    (20, 0),
    #(20, 1),
    (19, 0),
    (18, 0),
    (18, 120)
    #(19, 1)
]

for ring, pad in test_ring_pads_pairs:
    x,y = get_pad_center(ring, pad)
    ax.plot(x, y, 'o', label=f'Ring {ring}, Pad {pad}')

plt.show()