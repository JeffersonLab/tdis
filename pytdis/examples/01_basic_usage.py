"""Basic usage example for pytdis library."""

import pytdis

# Example 1: Read data as NumPy arrays
print("Example 1: Reading data as NumPy arrays")
print("-" * 40)

# Read first 10 events
tracks, hits = pytdis.read_tdis_data_numpy('detector_data.txt', n_events=10)

print(f"Read {len(tracks)} tracks")
print(f"Track data shape: {tracks.shape}")
print(f"Hits data shape: {hits.shape}")

# Access first track
if len(tracks) > 0:
    print(f"\nFirst track:")
    print(f"  Momentum: {tracks[0, 0]:.3f} GeV/c")
    print(f"  Theta: {tracks[0, 1]:.2f} degrees")
    print(f"  Phi: {tracks[0, 2]:.2f} degrees")
    print(f"  Z vertex: {tracks[0, 3]:.4f} m")

    # Count valid hits for first track
    import numpy as np
    valid_hits = ~np.isnan(hits[0, :, 0])
    n_hits = np.sum(valid_hits)
    print(f"  Number of hits: {n_hits}")

# Example 2: Read data as Pandas DataFrame
print("\n\nExample 2: Reading data as Pandas DataFrame")
print("-" * 40)

# Read events 5-15 (skip first 5, read 10)
df = pytdis.read_tdis_data_pandas('detector_data.txt', n_events=10, skip_events=5)

print(f"DataFrame shape: {df.shape}")
print(f"Columns: {list(df.columns)}")

# Get track summary
summary = pytdis.get_track_summary(df)
print(f"\nTrack summary:")
print(summary)

# Access specific track
if 0 in df.index.get_level_values('track_id'):
    track_0 = df.loc[0]
    print(f"\nTrack 0 has {len(track_0)} hits")
    print(f"Hit times range: {track_0['time'].min():.2f} - {track_0['time'].max():.2f} ns")

# Example 3: Working with hit data
print("\n\nExample 3: Analyzing hit data")
print("-" * 40)

# Get all hits from plane 5
plane_5_hits = df[df['plane'] == 5]
print(f"Number of hits in plane 5: {len(plane_5_hits)}")

# Average ADC by ring
if not df.empty:
    avg_adc_by_ring = df.groupby('ring')['adc'].mean()
    print("\nAverage ADC by ring:")
    print(avg_adc_by_ring)