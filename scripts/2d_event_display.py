import argparse
import math
import numpy as np
from bokeh.plotting import figure, show
from bokeh.models import ColumnDataSource, Slider, CustomJS
from bokeh.layouts import column, row
from bokeh.io import output_file

# Constants
num_rings = 21
num_pads_per_ring = 122
first_ring_inner_radius = 5.0  # Minimum radius in cm
max_radius = 15.0         # Maximum radius in cm
total_radius = max_radius - first_ring_inner_radius  # Total radial width (10 cm)
ring_width = total_radius / num_rings   # Radial width of each ring
delta_theta = 2 * math.pi / num_pads_per_ring  # Angular width of each pad in radians
plane_spacing_cm = 55.0 / 10  # 55 cm total length, 10 planes, so 5.5 cm per plane

def get_pad_center(ring, pad):
    """
    Compute the X and Y coordinates of the center of a pad given its ring and pad indices.
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
    theta_center = delta_theta / 2 + pad * delta_theta + theta_offset
    # Convert from polar to Cartesian coordinates
    x = r_center * np.cos(theta_center)
    y = r_center * np.sin(theta_center)
    return x, y

def read_track_data(filename):
    """
    Read the track data from a file and return track parameters and hits.
    """
    with open(filename, 'r') as f:
        lines = f.readlines()
    # Remove empty lines and strip whitespace
    lines = [line.strip() for line in lines if line.strip()]
    # Skip the "Event" line
    event_line = lines[0]
    track_param_line = lines[1]
    track_params = list(map(float, track_param_line.split()))
    momentum, theta_deg, phi_deg, z_vertex = track_params
    hits = []
    for line in lines[2:]:
        hit_data = list(map(float, line.split()))
        hits.append(hit_data)
    return momentum, theta_deg, phi_deg, z_vertex, hits

def prepare_hits_data(hits):
    """
    Prepare the hits data for plotting.
    """
    hit_times = []
    hit_xs = []
    hit_ys = []
    hit_zs = []
    amplitudes = []

    for hit in hits:
        # Unpack hit data
        time_arrival_ns = hit[0]
        amplitude = hit[1]
        ring = int(hit[2])
        pad = int(hit[3])
        plane = int(hit[4])
        ZtoGEM_m = hit[5]
        # Get x, y position
        x, y = get_pad_center(ring, pad)
        # Compute z position in cm
        z_hit_cm = plane * plane_spacing_cm + ZtoGEM_m * 100  # Convert ZtoGEM from m to cm
        # Append to lists
        hit_times.append(time_arrival_ns)
        hit_xs.append(x)
        hit_ys.append(y)
        hit_zs.append(z_hit_cm)
        amplitudes.append(amplitude)
    # Convert lists to numpy arrays
    hit_times = np.array(hit_times)
    hit_xs = np.array(hit_xs)
    hit_ys = np.array(hit_ys)
    hit_zs = np.array(hit_zs)
    amplitudes = np.array(amplitudes)
    return hit_times, hit_xs, hit_ys, hit_zs, amplitudes

def prepare_background_geometry():
    """
    Prepare the background geometry data for plotting.
    """
    pad_xs = []
    pad_ys = []
    pad_zs = []

    for ring in range(num_rings):
        for pad in range(num_pads_per_ring):
            x, y = get_pad_center(ring, pad)
            for plane in range(10):
                z = plane * plane_spacing_cm
                pad_xs.append(x)
                pad_ys.append(y)
                pad_zs.append(z)
    return pad_xs, pad_ys, pad_zs

def create_z_y_plot(hit_times, hit_xs, hit_ys, hit_zs, amplitudes, pad_xs, pad_ys, pad_zs, time_slider):
    """
    Create the Z vs Y plot with interactive slider using CustomJS.
    """
    # Create ColumnDataSource for background pads
    background_source = ColumnDataSource(data=dict(z=pad_zs, y=pad_ys))

    # Create ColumnDataSource for hits
    source = ColumnDataSource(data=dict(z=[], y=[]))

    # All hits data source
    all_hits_source = ColumnDataSource(data=dict(z=hit_zs, y=hit_ys, time=hit_times))

    # Create figure
    p = figure(title="Track Hits in TPC Detector (Z vs Y)", x_axis_label='z (cm)', y_axis_label='Y Position (cm)', width=800, height=600)

    # Plot background pads as small circles
    p.circle('z', 'y', size=2, color='lightgrey', alpha=0.3, source=background_source)

    # Plot hits
    hits_renderer = p.circle('z', 'y', size=6, color='red', source=source)

    # Define CustomJS callback
    callback = CustomJS(args=dict(source=source, all_source=all_hits_source, slider=time_slider), code="""
        const data = source.data;
        const all_data = all_source.data;
        const time = slider.value;

        const z = [];
        const y = [];

        for (let i = 0; i < all_data['time'].length; i++) {
            if (all_data['time'][i] <= time) {
                z.push(all_data['z'][i]);
                y.push(all_data['y'][i]);
            }
        }
        data['z'] = z;
        data['y'] = y;
        source.change.emit();
    """)

    time_slider.js_on_change('value', callback)

    return p

def create_x_y_plot(hit_times, hit_xs, hit_ys, hit_zs, amplitudes, pad_xs, pad_ys, pad_zs, time_slider):
    """
    Create the X vs Y plot with interactive slider using CustomJS.
    """
    # Create ColumnDataSource for background pads
    background_source = ColumnDataSource(data=dict(x=pad_xs, y=pad_ys))

    # Create ColumnDataSource for hits
    source = ColumnDataSource(data=dict(x=[], y=[]))

    # All hits data source
    all_hits_source = ColumnDataSource(data=dict(x=hit_xs, y=hit_ys, time=hit_times))

    # Create figure
    p = figure(title="Track Hits in TPC Detector (X vs Y)", x_axis_label='X Position (cm)', y_axis_label='Y Position (cm)', width=600, height=600,
               match_aspect=True)

    # Plot background pads as small circles
    p.circle('x', 'y', size=2, color='lightgrey', alpha=0.3, source=background_source)

    # Plot hits
    hits_renderer = p.circle('x', 'y', size=6, color='blue', source=source)

    # Define CustomJS callback
    callback = CustomJS(args=dict(source=source, all_source=all_hits_source, slider=time_slider), code="""
        const data = source.data;
        const all_data = all_source.data;
        const time = slider.value;

        const x = [];
        const y = [];

        for (let i = 0; i < all_data['time'].length; i++) {
            if (all_data['time'][i] <= time) {
                x.push(all_data['x'][i]);
                y.push(all_data['y'][i]);
            }
        }
        data['x'] = x;
        data['y'] = y;
        source.change.emit();
    """)

    time_slider.js_on_change('value', callback)

    return p

def create_plot(hit_times, hit_xs, hit_ys, hit_zs, amplitudes, pad_xs, pad_ys, pad_zs):
    """
    Create the combined Bokeh plot with interactive slider.
    """
    # Create a time slider
    time_slider = Slider(start=hit_times.min(), end=hit_times.max(), value=hit_times.min(), step=0.1, title="Time (ns)")

    # Create the Z vs Y plot
    z_y_plot = create_z_y_plot(hit_times, hit_xs, hit_ys, hit_zs, amplitudes, pad_xs, pad_ys, pad_zs, time_slider)

    # Create the X vs Y plot
    x_y_plot = create_x_y_plot(hit_times, hit_xs, hit_ys, hit_zs, amplitudes, pad_xs, pad_ys, pad_zs, time_slider)

    # Layout
    layout = column(row(z_y_plot, x_y_plot), time_slider)

    # Show plot
    show(layout)

def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description='Plot TPC track data.')
    parser.add_argument('filename', help='Track data file')
    args = parser.parse_args()

    # Read the track data
    momentum, theta_deg, phi_deg, z_vertex, hits = read_track_data(args.filename)

    # Prepare hits data
    hit_times, hit_xs, hit_ys, hit_zs, amplitudes = prepare_hits_data(hits)

    # Prepare background geometry
    pad_xs, pad_ys, pad_zs = prepare_background_geometry()

    # Create and show the plot
    create_plot(hit_times, hit_xs, hit_ys, hit_zs, amplitudes, pad_xs, pad_ys, pad_zs)

if __name__ == '__main__':
    main()
