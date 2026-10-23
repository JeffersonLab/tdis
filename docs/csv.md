# CSV Data Format

The TDIS tracking system exports comprehensive tracking data to CSV format for analysis, debugging, and performance evaluation.

## Overview

- CSV files are generated with configurable prefix via `csv:prefix` parameter (default: "output")
- Five interconnected CSV files are created per run:
  - `{prefix}.seeds.csv` - Track seeds with Monte Carlo truth and initial parameters
  - `{prefix}.hits.csv` - Hit measurements and detailed hit information
  - `{prefix}.fitted_tracks.csv` - Fitted track results after Kalman filtering
  - `{prefix}.track_states.csv` - Detailed track states along trajectories
  - `{prefix}.comparison.csv` - Performance comparison between truth and fitted values
- Column names are provided in the first line (header row) of each CSV file
- All numeric values use standard scientific notation where appropriate
- Files are linked through common ID fields enabling full traceability

## ID Linkage Strategy

The CSV files form a relational structure with full traceability from MC truth through reconstruction:

```mermaid
erDiagram
    Events ||--o{ Track_Seeds : contains
    Track_Seeds ||--o{ Hit_Measurements : has
    Track_Seeds ||--o| Fitted_Tracks : produces
    Fitted_Tracks ||--o{ Track_States : contains
    Track_Seeds ||--o| Comparison : evaluated_in
    
    Track_Seeds {
        int evt PK "Event Number"
        int seed_id PK "Seed Index"
        float mc_mom "MC Momentum"
        float init_params "Initial Parameters"
        float perigee "Perigee Point"
    }
    
    Hit_Measurements {
        int evt FK "Event Number"
        int seed_id FK "Seed Index"
        int meas_id PK "Measurement Index"
        float meas_data "Measurement Data"
        float mc_hit_data "MC Hit Truth"
    }
    
    Fitted_Tracks {
        int evt FK "Event Number"
        int track_id PK "Track Index"
        int seed_id FK "Original Seed"
        float fit_momentum "Fitted Momentum"
        float chi2_ndf "Fit Quality"
    }
    
    Track_States {
        int evt FK "Event Number"
        int track_id FK "Track Index"
        int seed_id FK "Seed Index"
        int state_idx PK "State Index"
        float state_params "State Parameters"
    }
    
    Comparison {
        int evt FK "Event Number"
        int seed_id FK "Seed Index"
        float mc_values "MC Truth"
        float fit_values "Fitted Results"
        float deltas "Residuals"
    }
```

## Table Definitions

### seeds.csv

Contains track seed information with Monte Carlo truth data and initial track parameters.

**Primary Key:** Composite key of (`evt`, `seed_id`)

| Column | Name | Type | Description |
|--------|------|------|-------------|
| 0 | `evt` | uint64 | Event number/index |
| 1 | `seed_id` | int | Track seed index within event |
| 2 | `mc_mom` | float | MC track total momentum [GeV/c] |
| 3 | `mc_phi` | float | MC track phi angle [rad] |
| 4 | `mc_theta` | float | MC track theta angle [rad] |
| 5 | `mc_vtx_z` | float | MC track Z vertex [mm] |
| 6 | `mc_hits_count` | int | Number of MC hits |
| 7 | `pdg` | int | Particle PDG code |
| 8 | `init_phi` | float | Initial parameters phi [rad] |
| 9 | `init_theta` | float | Initial parameters theta [rad] |
| 10 | `init_time` | float | Initial track time [ns] |
| 11 | `init_qoverp` | float | Initial charge/momentum [c/GeV] |
| 12 | `init_surface` | uint64 | Initial surface geometry ID |
| 13 | `init_loc0` | float | Initial local position 0 [mm] |
| 14 | `init_loc1` | float | Initial local position 1 [mm] |
| 15-20 | `cov_*` | float | Covariance matrix diagonal elements |
| 21-23 | `perigee_*` | float | Perigee point coordinates [mm] |
| 24-32 | `fhit_*` | various | First hit information (optional) |

### hits.csv

Contains measurement and hit information for each track seed.

**Primary Key:** Composite key of (`evt`, `seed_id`, `meas_id`)

| Column | Name | Type | Description |
|--------|------|------|-------------|
| 0 | `evt` | uint64 | Event number |
| 1 | `seed_id` | int | Track seed index (FK to seeds) |
| 2 | `meas_id` | int | Measurement index within track |
| 3 | `meas_time` | float | Measurement time [ns] |
| 4 | `meas_surface` | uint64 | Surface geometry ID |
| 5-6 | `meas_loc*` | float | Local position [mm] |
| 7-9 | `meas_cov*` | float | Measurement covariances |
| 10-16 | `hit_*` | various | Reconstructed hit data |
| 17-26 | `mc_hit_*` | various | MC truth hit data |

### fitted_tracks.csv

Contains the fitted track at closest approach to the z-axis through the origin.
`fit_type=0` means the inward extrapolation succeeded; `fit_type=1` means it
failed and the output remains at the fitted inner cylinder. Position, momentum,
time, and covariance refer to the same output state. Extrapolation transports
covariance through the configured magnetic field and includes no material effects.
A perigee position is not an independently reconstructed interaction vertex.

**Primary Key:** (`evt`, `track_id`); `seed_id` refers to the original seed.

| Column | Name | Description |
|--------|------|-------------|
| 0-2 | `evt`, `track_id`, `seed_id` | Event and object identifiers |
| 3-6 | `mc_p`, `mc_theta`, `mc_phi`, `mc_vtx_z` | Truth momentum [GeV], angles [rad], vertex z [mm] |
| 7-9 | `init_p`, `init_theta`, `init_phi` | Seed momentum [GeV] and angles [rad] |
| 10-12 | `fit_p`, `fit_theta`, `fit_phi` | Fitted momentum [GeV] and angles [rad] |
| 13 | `fit_pos_z` | Output position z [mm] |
| 14 | `fit_chi2ndf` | χ² divided by measured-coordinate count (legacy convention) |
| 15 | `fit_type` | 0 perigee reached; 1 cylinder fallback |
| 16-18 | `fit_px`, `fit_py`, `fit_pz` | Momentum components [GeV] |
| 19-20 | `fit_pos_x`, `fit_pos_y` | Output position x and y [mm] |
| 21-23 | `fit_time`, `fit_time_err`, `fit_charge` | Time and uncertainty [ns], charge [e] |
| 24-26 | `fit_chi2`, `fit_ndf`, `fit_pdg` | Fit quality and particle hypothesis |
| 27-30 | `n_states`, `n_measurements`, `n_outliers`, `n_holes` | KF trajectory counts |
| 31 | `fit_surface` | Primary geometry ID; 0 denotes the origin beamline perigee |

Schema 102 renames the former `fit_vtx_x/y/z` columns to `fit_pos_x/y/z`;
column positions are preserved and `fit_surface` is appended. Consumers should
check `fit_type` before interpreting the position as a perigee.

The legacy `fit_ndf` counts measured coordinates (2 × accepted 2D measurements),
so `fit_chi2ndf` is not conventional reduced χ². No χ² selection is applied by
the performance workflow.

### track_states.csv

Contains reference states, not the full predicted/filtered/smoothed history.
On extrapolation success, state 0 is the perigee and state 1 is the retained
fitted cylinder. On failure, only the cylinder is stored. These rows correspond
to the trajectory's `TrackParameters` relations in ROOT; parameter type 0 is
perigee and 1 is cylinder. Surface ID 0 is the z-axis perigee through the origin;
its local coordinates are ACTS signed d0 and z0 [mm]. Nonzero IDs identify the
cylinders, whose local coordinates are Rφ and z [mm].

**Primary Key:** Composite key of (`evt`, `track_id`, `state_idx`)

| Column | Name | Type | Description |
|--------|------|------|-------------|
| 0 | `evt` | uint64 | Event number |
| 1 | `track_id` | int | Track ID (FK) |
| 2 | `seed_id` | int | Original seed ID (FK) |
| 3 | `state_idx` | int | State index along track |
| 4 | `surface_id` | uint64 | Surface geometry ID |
| 5-6 | `loc*` | float | Local position [mm] |
| 7 | `phi` | float | Phi angle [rad] |
| 8 | `theta` | float | Theta angle [rad] |
| 9 | `qoverp` | float | Charge/momentum [c/GeV] |
| 10 | `p` | float | Momentum magnitude [GeV/c] |
| 11 | `time` | float | Time [ns] |
| 12 | `path_length` | float | Unavailable (`nan`) |
| 13 | `chi2` | float | Unavailable for reference states (`nan`) |
| 14 | `type` | string | Reference type (`perigee`/`cylinder`) |

### comparison.csv

Side-by-side comparison of MC truth, initial, and fitted parameters.

**Primary Key:** Composite key of (`evt`, `seed_id`)

| Column | Name | Type | Description |
|--------|------|------|-------------|
| 0 | `evt` | uint64 | Event number |
| 1 | `seed_id` | int | Seed ID (FK) |
| 2-5 | `mc_*` | float | MC truth values |
| 6-8 | `init_*` | float | Initial seed parameters |
| 9-12 | `fit_*` | float | Fitted parameters |
| 13 | `fit_chi2ndf` | float | Fit quality |
| 14 | `delta_p` | float | Momentum residual [GeV/c] |
| 15 | `delta_p_rel` | float | Relative momentum residual |
| 16 | `delta_theta` | float | Theta residual [rad] |
| 17 | `delta_phi` | float | Phi residual [rad] |
| 18 | `delta_vtx_z` | float | Vertex Z residual [mm] |
| 19 | `fit_success` | int | Fit success flag (1/0) |

## Usage Examples

### Python/Pandas Analysis

```python
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Load all CSV files
seeds_df = pd.read_csv('output.seeds.csv')
hits_df = pd.read_csv('output.hits.csv')
tracks_df = pd.read_csv('output.fitted_tracks.csv')
states_df = pd.read_csv('output.track_states.csv')
comp_df = pd.read_csv('output.comparison.csv')

# Example 1: Track reconstruction efficiency
total_seeds = len(seeds_df)
fitted_tracks = len(tracks_df)
efficiency = fitted_tracks / total_seeds * 100
print(f"Tracking efficiency: {efficiency:.1f}%")

# Example 2: Momentum resolution study
comp_df['p_resolution'] = comp_df['delta_p_rel'] * 100  # Convert to percentage
plt.hist(comp_df['p_resolution'], bins=50, range=(-10, 10))
plt.xlabel('Momentum Resolution (%)')
plt.ylabel('Tracks')
plt.title(f'Momentum Resolution (RMS: {comp_df["p_resolution"].std():.2f}%)')

# Example 3: Chi2/ndf distribution for quality assessment
quality_tracks = tracks_df[tracks_df['fit_chi2ndf'] > 0]
plt.hist(quality_tracks['fit_chi2ndf'], bins=50, range=(0, 10))
plt.xlabel('χ²/ndf')
plt.ylabel('Tracks')
plt.axvline(x=1, color='r', linestyle='--', label='Expected')

# Example 4: Residuals per layer
residuals_by_layer = states_df.groupby('surface_id').agg({
    'chi2': 'mean',
    'state_idx': 'count'
})
print("Average chi2 per detector layer:")
print(residuals_by_layer)

# Example 5: Track parameter pulls
# Join seeds with comparison to get covariances
pulls_df = pd.merge(seeds_df, comp_df, on=['evt', 'seed_id'])
pulls_df['pull_theta'] = pulls_df['delta_theta'] / np.sqrt(pulls_df['cov_theta'])
pulls_df['pull_phi'] = pulls_df['delta_phi'] / np.sqrt(pulls_df['cov_phi'])

# Example 6: Outlier analysis
outlier_tracks = tracks_df[tracks_df['n_outliers'] > 0]
print(f"Tracks with outliers: {len(outlier_tracks)} / {len(tracks_df)}")
```


## Performance Metrics

Common analysis metrics extractable from the CSV data:

1. **Tracking Efficiency**: `fitted_tracks / total_seeds`
2. **Momentum Resolution**: Statistics of `delta_p_rel` in comparison.csv
3. **Angular Resolution**: Statistics of `delta_theta`, `delta_phi`
4. **Track Quality**: Distribution of `fit_chi2ndf`
5. **Hit Efficiency**: `n_measurements / mc_hits_count`
6. **Outlier Rate**: `n_outliers / (n_measurements + n_outliers)`
7. **Vertex Resolution**: Statistics of `delta_vtx_z`

## Units

All units follow the Acts framework conventions:

- **Length**: millimeters (mm)
- **Time**: nanoseconds (ns)
- **Momentum**: GeV/c
- **Angles**: radians
- **Charge**: elementary charge units (e)
- **Energy**: GeV

## Implementation

The enhanced CSV writer is implemented in:
- Source: [`source/tdis/io/CsvWriterProcessor.hpp`](source/tdis/io/CsvWriterProcessor.hpp)
- Track Fitting: [`source/tdis/tracking/KalmanFittingFactory.cpp`](source/tdis/tracking/KalmanFittingFactory.cpp)

The writer processes:
- Input: `TrackSeed` objects with `Measurement2D` and MC truth
- Output: `Track`, `Trajectory`, `TrackParameters` from Kalman filter
- Links all data through consistent ID relationships for full traceability
