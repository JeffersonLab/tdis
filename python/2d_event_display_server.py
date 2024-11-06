import os
import math
import numpy as np
from bokeh.plotting import figure
from bokeh.models import ColumnDataSource, Slider, Select, Button
from bokeh.layouts import column, row
from bokeh.io import curdoc

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
    Read the track data from a file and return a list of events.
    Each event is a dictionary with keys: 'event_number', 'track_params', 'hits'.
    """
    events = []
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
            event = {
                'event_number': event_number,
                'track_params': track_params,
                'hits': hits
            }
            events.append(event)
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

def create_plots(events):
    """
    Create the plots and return the layout.
    """
    # Prepare background geometry
    pad_xs, pad_ys, pad_zs = prepare_background_geometry()

    # Create ColumnDataSource for background pads
    background_source_xy = ColumnDataSource(data=dict(x=pad_xs, y=pad_ys))
    background_source_zy = ColumnDataSource(data=dict(z=pad_zs, y=pad_ys))

    # Create empty ColumnDataSource for hits
    source_xy = ColumnDataSource(data=dict(x=[], y=[]))
    source_zy = ColumnDataSource(data=dict(z=[], y=[]))
    all_hits_source = ColumnDataSource(data=dict(x=[], y=[], z=[], time=[]))

    # Create figures
    p_xy = figure(title="Track Hits in TPC Detector (X vs Y)", x_axis_label='X Position (cm)', y_axis_label='Y Position (cm)', width=600, height=600,
                  match_aspect=True)
    p_zy = figure(title="Track Hits in TPC Detector (Z vs Y)", x_axis_label='Z Position (cm)', y_axis_label='Y Position (cm)', width=800, height=600)

    # Plot background pads
    p_xy.circle('x', 'y', size=2, color='lightgrey', alpha=0.3, source=background_source_xy)
    p_zy.circle('z', 'y', size=2, color='lightgrey', alpha=0.3, source=background_source_zy)

    # Plot hits
    p_xy.circle('x', 'y', size=6, color='blue', source=source_xy)
    p_zy.circle('z', 'y', size=6, color='red', source=source_zy)

    # Create time slider
    time_slider = Slider(start=0, end=1, value=0, step=0.1, title="Time (ns)")

    # Create event selector
    event_numbers = [str(event['event_number']) for event in events]
    event_selector = Select(title="Select Event:", value=event_numbers[0], options=event_numbers)

    # Define the callback to load event data
    def load_event_data(attr, old, new):
        # Get the selected event
        selected_event_number = int(event_selector.value)
        selected_event = next(event for event in events if event['event_number'] == selected_event_number)
        hits = selected_event['hits']
        # Prepare hits data
        hit_times, hit_xs, hit_ys, hit_zs, amplitudes = prepare_hits_data(hits)
        # Update all_hits_source
        all_hits_source.data = dict(x=hit_xs, y=hit_ys, z=hit_zs, time=hit_times)
        # Update time slider range
        time_slider.start = hit_times.min()
        time_slider.end = hit_times.max()
        time_slider.value = hit_times.min()
        # Update the plots
        update_plots(None, None, None)

    event_selector.on_change('value', load_event_data)

    # Define the callback to update plots based on time slider
    def update_plots(attr, old, new):
        current_time = time_slider.value
        data = all_hits_source.data
        mask = data['time'] <= current_time
        source_xy.data = dict(x=np.array(data['x'])[mask], y=np.array(data['y'])[mask])
        source_zy.data = dict(z=np.array(data['z'])[mask], y=np.array(data['y'])[mask])

    time_slider.on_change('value', update_plots)

    # Initial load of event data
    load_event_data(None, None, None)

    # Arrange layout
    layout = column(event_selector, row(p_zy, p_xy), time_slider)
    return layout

# Read the track data from the file
filename = 'D:\\data\\tdis\\g4sbsout_EPCEvents_200000.txt'  # Replace with the actual filename

print("Reading events...")
events = read_track_data(filename)
print(f"Loaded events: {len(events)}")

# Create and add the layout to the document
layout = create_plots(events)
curdoc().add_root(layout)
curdoc().title = "TPC Event Display"
