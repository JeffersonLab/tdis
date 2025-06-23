"""
I/O functions for reading TDIS mTPC detector data files.

This module provides functions to read and parse TDIS experiment detector data files
in both NumPy array and Pandas DataFrame formats.
"""

import numpy as np
import pandas as pd
from typing import Tuple, Optional, List, Dict, Iterator


def parse_event_data(lines: List[str], event_num: int) -> Tuple[np.ndarray, List[np.ndarray]]:
    """
    Parse a single event from the data lines.

    Parameters
    ----------
    lines : List[str]
        Lines containing the event data (excluding the "Event N" line)
    event_num : int
        Event number for error reporting

    Returns
    -------
    track_info : np.ndarray
        Array of shape (4,) containing [momentum, theta, phi, z_vertex]
    hits : List[np.ndarray]
        List of hit arrays, each of shape (9,) containing hit data
        [time, adc, true_x, true_y, true_z, ring, pad, plane, z_to_gem]
        where ring, pad, plane are integers

    Raises
    ------
    ValueError
        If the event data format is invalid
    """
    if not lines:
        raise ValueError(f"Event {event_num} has no data")

    # Parse track information (first line)
    track_parts = lines[0].strip().split()
    if len(track_parts) != 4:
        raise ValueError(f"Event {event_num}: Expected 4 track parameters, got {len(track_parts)}")

    track_info = np.array([float(x) for x in track_parts])

    # Parse hit information (remaining lines)
    hits = []
    for i, line in enumerate(lines[1:], 1):
        hit_parts = line.strip().split()
        if len(hit_parts) != 9:
            raise ValueError(f"Event {event_num}, Hit {i}: Expected 9 hit parameters, got {len(hit_parts)}")

        # Convert to appropriate types
        # Fields: time, adc, true_x, true_y, true_z, ring, pad, plane, z_to_gem
        hit_data = np.array([
            float(hit_parts[0]),  # time
            float(hit_parts[1]),  # adc
            float(hit_parts[2]),  # true_x
            float(hit_parts[3]),  # true_y
            float(hit_parts[4]),  # true_z
            int(hit_parts[5]),    # ring (integer)
            int(hit_parts[6]),    # pad (integer)
            int(hit_parts[7]),    # plane (integer)
            float(hit_parts[8])   # z_to_gem
        ])
        hits.append(hit_data)

    return track_info, hits


def _read_events_iterator(filename: str,
                          skip_events: int = 0) -> Iterator[Tuple[int, np.ndarray, List[np.ndarray]]]:
    """
    Internal generator function that yields parsed events from a file.

    Parameters
    ----------
    filename : str
        Path to the data file
    skip_events : int
        Number of events to skip from the beginning

    Yields
    ------
    track_id : int
        Sequential track ID starting from 0
    track_info : np.ndarray
        Track information array
    hits : List[np.ndarray]
        List of hit arrays for this track
    """
    with open(filename, 'r') as f:
        event_count = 0
        track_id = 0
        current_event_lines = []
        in_event = False

        for line in f:
            # Check if this is an event marker
            if line.strip().startswith("Event"):
                # Process previous event if exists
                if in_event and current_event_lines:
                    if event_count >= skip_events:
                        try:
                            track_info, hits = parse_event_data(current_event_lines, event_count)
                            yield track_id, track_info, hits
                            track_id += 1
                        except ValueError as e:
                            print(f"Warning: {e}")

                event_count += 1
                current_event_lines = []
                in_event = True
            elif in_event and line.strip():
                current_event_lines.append(line)

        # Process last event if exists
        if in_event and current_event_lines and event_count >= skip_events:
            try:
                track_info, hits = parse_event_data(current_event_lines, event_count)
                yield track_id, track_info, hits
            except ValueError as e:
                print(f"Warning: {e}")


def read_tdis_data_numpy(filename: str,
                         n_events: Optional[int] = None,
                         skip_events: int = 0) -> Tuple[np.ndarray, np.ndarray]:
    """
    Read TDIS detector data and return as NumPy arrays.

    Parameters
    ----------
    filename : str
        Path to the data file
    n_events : int, optional
        Number of events to read (None for all events)
    skip_events : int
        Number of events to skip from the beginning

    Returns
    -------
    tracks : np.ndarray
        2D array of shape (n_tracks, 4) containing track information
        Columns: [momentum, theta, phi, z_vertex]
    hits : np.ndarray
        3D array of shape (n_tracks, max_hits, 9) containing hit information
        Columns: [time, adc, true_x, true_y, true_z, ring, pad, plane, z_to_gem]
        Note: Unused hit slots are filled with NaN
    """
    tracks_list = []
    hits_list = []
    events_read = 0

    # Use the iterator to read events
    for track_id, track_info, hits in _read_events_iterator(filename, skip_events):
        if n_events is not None and events_read >= n_events:
            break

        tracks_list.append(track_info)
        hits_list.append(hits)
        events_read += 1

    # Convert to numpy arrays
    tracks = np.array(tracks_list) if tracks_list else np.array([]).reshape(0, 4)

    # Create 3D array for hits with padding
    if hits_list:
        max_hits = max(len(h) for h in hits_list)
        n_tracks = len(hits_list)

        hits_array = np.full((n_tracks, max_hits, 9), np.nan)
        for i, event_hits in enumerate(hits_list):
            n_hits = len(event_hits)
            if n_hits > 0:
                hits_array[i, :n_hits, :] = np.array(event_hits)
    else:
        hits_array = np.array([]).reshape(0, 0, 9)

    return tracks, hits_array


def read_tdis_data_pandas(filename: str,
                          n_events: Optional[int] = None,
                          skip_events: int = 0) -> pd.DataFrame:
    """
    Read TDIS detector data and return as a Pandas DataFrame.

    Parameters
    ----------
    filename : str
        Path to the data file
    n_events : int, optional
        Number of events to read (None for all events)
    skip_events : int
        Number of events to skip from the beginning

    Returns
    -------
    df : pd.DataFrame
        DataFrame with multi-index (track_id, hit_id) containing all data
        Track columns: momentum, theta, phi, z_vertex
        Hit columns: time, adc, true_x, true_y, true_z, ring, pad, plane, z_to_gem
    """
    data_records = []
    events_read = 0

    # Use the iterator to read events
    for track_id, track_info, hits in _read_events_iterator(filename, skip_events):
        if n_events is not None and events_read >= n_events:
            break

        # Add records for this track
        for hit_id, hit in enumerate(hits):
            record = {
                'track_id': track_id,
                'hit_id': hit_id,
                'momentum': track_info[0],
                'theta': track_info[1],
                'phi': track_info[2],
                'z_vertex': track_info[3],
                'time': hit[0],
                'adc': hit[1],
                'true_x': hit[2],
                'true_y': hit[3],
                'true_z': hit[4],
                'ring': int(hit[5]),     # Ensure integer
                'pad': int(hit[6]),      # Ensure integer
                'plane': int(hit[7]),    # Ensure integer
                'z_to_gem': hit[8]
            }
            data_records.append(record)

        events_read += 1

    # Create DataFrame
    df = pd.DataFrame(data_records)

    if not df.empty:
        # Set multi-index
        df = df.set_index(['track_id', 'hit_id'])

        # Ensure correct data types
        for col in ['ring', 'pad', 'plane']:
            df[col] = df[col].astype(int)

    return df


def get_track_summary(df: pd.DataFrame) -> pd.DataFrame:
    """
    Get a summary of tracks from the DataFrame.

    Parameters
    ----------
    df : pd.DataFrame
        DataFrame returned by read_tdis_data_pandas

    Returns
    -------
    summary : pd.DataFrame
        Summary with one row per track containing track info and hit statistics
    """
    if df.empty:
        return pd.DataFrame()

    # Group by track_id and aggregate
    track_cols = ['momentum', 'theta', 'phi', 'z_vertex']
    summary = df.groupby(level='track_id').agg({
        **{col: 'first' for col in track_cols},
        'time': ['count', 'min', 'max'],
        'adc': ['mean', 'sum'],
        'ring': lambda x: len(x.unique()),
        'pad': lambda x: len(x.unique()),
        'plane': lambda x: len(x.unique())
    })

    # Flatten column names
    summary.columns = ['_'.join(col).strip('_') if col[1] else col[0]
                       for col in summary.columns.values]

    # Rename some columns for clarity
    summary = summary.rename(columns={
        'time_count': 'n_hits',
        'time_min': 'time_first',
        'time_max': 'time_last',
        'adc_mean': 'adc_avg',
        'adc_sum': 'adc_total',
        'ring_<lambda>': 'n_rings',
        'pad_<lambda>': 'n_pads',
        'plane_<lambda>': 'n_planes'
    })

    return summary