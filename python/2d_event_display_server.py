"""
Interactive 2d event display for text digitization files for tdis
One needs to 'pip install bokeh numpy' to use it
How to use:
  > bokeh serve --show 2d_event_display_server.py --args track_data.txt

With IDEs
If you use it in IDEs like CLion or PyCharm one can use
  Module: bokeh
  Command line options: "serve --show 2d_event_display_server.py --args  track_data.txt"
"""

import sys
import math
import numpy as np
from bokeh.plotting import figure
from bokeh.models import (
    ColumnDataSource, Slider, Button, TextInput, Div,
    CategoricalColorMapper
)
from bokeh.layouts import column, row
from bokeh.io import curdoc
from bokeh.palettes import Category10_10


# Constants
num_rings = 21
num_pads_per_ring = 122
first_ring_inner_radius = 5.0  # Minimum radius in cm
max_radius = 15.0              # Maximum radius in cm
total_radius = max_radius - first_ring_inner_radius  # Total radial width (10 cm)
ring_width = total_radius / num_rings                # Radial width of each ring
delta_theta = 2 * math.pi / num_pads_per_ring        # Angular width of each pad in radians
plane_spacing_cm = 5.0  # Plane spacing is 5 cm
filename = 'track_data.txt'  # Default filename if no script arguments are provided


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
    theta_offset = 0 if ring % 2 == 0 else delta_theta / 2
    # Compute the angular center of the pad
    theta_center = delta_theta / 2 + pad * delta_theta + theta_offset
    # Convert from polar to Cartesian coordinates
    x = r_center * np.cos(theta_center)
    y = r_center * np.sin(theta_center)
    return x, y

def read_track_data(filename):
    """
    Read the track data from a file and return a dictionary of events.
    Each event is a dictionary with keys: 'track_params', 'hits'.
    """
    events = {}
    with open(filename, 'r') as f:
        lines = f.readlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith('Event'):
            event_number = int(line.split()[1])
            i += 1
            if i >= len(lines):
                break
            track_param_line = lines[i].strip()
            track_params = list(map(float, track_param_line.split()))
            i += 1
            hits = []
            while i < len(lines) and not lines[i].strip().startswith('Event'):
                hit_line = lines[i].strip()
                if hit_line:
                    hit_data = list(map(float, hit_line.split()))
                    hits.append(hit_data)
                i += 1
            events[event_number] = {
                'track_params': track_params,
                'hits': hits
            }
        else:
            i += 1
    return events

def prepare_hits_data(hits):
    """
    Prepare the hits data for plotting.
    """
    hit_times = []
    hit_xs = []
    hit_ys = []
    hit_zs = []
    amplitudes = []
    planes = []

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
        dz = ZtoGEM_m if plane % 2 == 0 else -ZtoGEM_m
        z_hit_cm = plane * plane_spacing_cm + dz * 100  # Convert ZtoGEM from m to cm
        # Append to lists
        hit_times.append(time_arrival_ns)
        hit_xs.append(x)
        hit_ys.append(y)
        hit_zs.append(z_hit_cm)
        amplitudes.append(amplitude)
        planes.append(f"Plane {plane}")
    # Convert lists to numpy arrays
    hit_times = np.array(hit_times)
    hit_xs = np.array(hit_xs)
    hit_ys = np.array(hit_ys)
    hit_zs = np.array(hit_zs)
    amplitudes = np.array(amplitudes)
    planes = np.array(planes)
    return hit_times, hit_xs, hit_ys, hit_zs, amplitudes, planes

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
                z = plane * plane_spacing_cm  # Adjusted plane positions
                pad_xs.append(x)
                pad_ys.append(y)
                pad_zs.append(z)
    return pad_xs, pad_ys, pad_zs

def create_plots(events):
    """
    Create the plots and return the layout.
    """
    # Prepare background geometry for XY plot (unchanged)
    pad_xs, pad_ys, pad_zs = prepare_background_geometry()
    background_source_xy = ColumnDataSource(data=dict(x=pad_xs, y=pad_ys))

    # Create empty ColumnDataSource for hits
    source_xy = ColumnDataSource(data=dict(x=[], y=[], plane=[]))
    source_zy = ColumnDataSource(data=dict(z=[], y=[], plane=[]))
    all_hits_source = ColumnDataSource(data=dict(x=[], y=[], z=[], time=[], plane=[]))

    # Create figures
    p_xy = figure(title="Track Hits in TPC Detector (X vs Y)", x_axis_label='X Position (cm)',
                  y_axis_label='Y Position (cm)', width=600, height=600, match_aspect=True)
    p_zy = figure(title="Track Hits in TPC Detector (Z vs Y)", x_axis_label='Z Position (cm)',
                  y_axis_label='Y Position (cm)', width=800, height=600)

    # Plot background pads for XY plot (unchanged)
    p_xy.circle('x', 'y', size=2, color='lightgrey', alpha=0.3, source=background_source_xy)

    # Define color mapper for planes
    planes_list = [f"Plane {i}" for i in range(10)]
    color_mapper = CategoricalColorMapper(factors=planes_list, palette=Category10_10)

    # **New Code: Prepare background rectangles for ZY plot**
    x_centers = []
    y_centers = []
    widths = []
    heights = []
    planes_bg = []

    Y_min = -max_radius
    Y_max = max_radius
    y_center = 0
    height = Y_max - Y_min  # Total height covering the detector in Y

    for plane in range(10):
        z_plane = plane * plane_spacing_cm
        x_center = z_plane  # Center of the plane in Z
        width = 0.5  # Width equal to plane spacing

        x_centers.append(x_center)
        y_centers.append(y_center)
        widths.append(width)
        heights.append(height)
        planes_bg.append(f"Plane {plane}")

    background_rect_source = ColumnDataSource(data=dict(
        x=x_centers,
        y=y_centers,
        width=widths,
        height=heights,
        plane=planes_bg
    ))

    # **Plot background rectangles in ZY plot**
    p_zy.rect('x', 'y', 'width', 'height', source=background_rect_source,
              fill_color={'field': 'plane', 'transform': color_mapper},
              line_color=None, fill_alpha=0.1)

    # **Remove the old background pads in ZY plot**
    # (Comment out or delete the following line)
    # p_zy.circle('z', 'y', size=2, color='lightgrey', alpha=0.3, source=background_source_zy)

    # Plot hits with colors based on plane
    hits_xy = p_xy.circle('x', 'y', size=6, source=source_xy,
                          color={'field': 'plane', 'transform': color_mapper}, legend_field='plane')
    hits_zy = p_zy.circle('z', 'y', size=6, source=source_zy,
                          color={'field': 'plane', 'transform': color_mapper}, legend_field='plane')

    # Adjust legends (unchanged)
    for plot in [p_xy, p_zy]:
        plot.legend.title = 'Planes'
        plot.legend.location = 'top_right'
        plot.legend.click_policy = 'hide'

    # Rest of the code in create_plots remains unchanged...


    # Create time slider
    time_slider = Slider(start=0, end=1, value=0, step=0.1, title="Time (ns)")

    # Create event number input and buttons
    min_event_number = min(events.keys())
    max_event_number = max(events.keys())

    event_number_input = TextInput(title="Event Number:", value=str(min_event_number), width=100)
    show_button = Button(label="Show", button_type="success", width=60)
    next_button = Button(label="Next Track", button_type="primary", width=80)

    # Create event limits display
    event_limits_div = Div(text=f"File {filename} has events: {min_event_number} to {max_event_number}", width=300)

    # Arrange controls
    controls = row(event_number_input, show_button, next_button, event_limits_div)

    # Keep track of the current event index and event numbers
    event_numbers = sorted(events.keys())
    current_event_index = 0  # index into event_numbers list

    # Define the function to load event data
    def load_event_data():
        nonlocal current_event_index
        # Get the event number from the input
        try:
            selected_event_number = int(event_number_input.value)
        except ValueError:
            print(f"Invalid event number: {event_number_input.value}")
            return

        if selected_event_number in events:
            selected_event = events[selected_event_number]
            current_event_index = event_numbers.index(selected_event_number)
        else:
            print(f"Event number {selected_event_number} not found.")
            return

        hits = selected_event['hits']
        # Prepare hits data
        hit_times, hit_xs, hit_ys, hit_zs, amplitudes, planes = prepare_hits_data(hits)
        # Update all_hits_source
        all_hits_source.data = dict(x=hit_xs, y=hit_ys, z=hit_zs, time=hit_times, plane=planes)
        # Update time slider range and set to maximum time
        time_slider.start = hit_times.min()
        time_slider.end = hit_times.max()
        time_slider.value = hit_times.max()  # Show all hits initially
        # Update the plots
        update_plots(None, None, None)

    # Define the callback for the "Show" button
    def on_show_button_click():
        load_event_data()

    show_button.on_click(on_show_button_click)

    # Define the callback for the "Next Track" button
    def on_next_button_click():
        nonlocal current_event_index
        current_event_index += 1
        if current_event_index >= len(event_numbers):
            current_event_index = 0  # Loop back to the first event
        selected_event_number = event_numbers[current_event_index]
        event_number_input.value = str(selected_event_number)
        load_event_data()

    next_button.on_click(on_next_button_click)

    # Define the callback to update plots based on time slider
    def update_plots(attr, old, new):
        current_time = time_slider.value
        data = all_hits_source.data
        mask = data['time'] <= current_time
        source_xy.data = dict(
            x=np.array(data['x'])[mask],
            y=np.array(data['y'])[mask],
            plane=np.array(data['plane'])[mask]
        )
        source_zy.data = dict(
            z=np.array(data['z'])[mask],
            y=np.array(data['y'])[mask],
            plane=np.array(data['plane'])[mask]
        )

    time_slider.on_change('value', update_plots)

    # Initial load of event data
    load_event_data()

    # Arrange layout
    layout = column(controls, row(p_zy, p_xy), time_slider)
    return layout




# bokeh serve set sys.argv for the script. I.e.
# If command line is:
# bokeh serve --show 2d_event_display_server.py --args track_data.txt
# Then sys.argv for this script will be
# ['2d_event_display_server.py', 'track_data.txt']
if len(sys.argv) > 1:
    filename = sys.argv[1]

print(f"Reading events from file: {filename}")
events = read_track_data(filename)
print(f"Loaded events: {len(events)}")

# Create and add the layout to the document
layout = create_plots(events)
curdoc().add_root(layout)
curdoc().title = "TPC Event Display"
