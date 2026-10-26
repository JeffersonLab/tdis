# TDIS output schema

The schema defines the saved objects, field types, units and relationships.
Its authoritative definition is [source/tdis/edm.yaml](../source/tdis/edm.yaml).
[CMake](../source/tdis/CMakeLists.txt) uses PODIO to generate the C++ classes
and ROOT dictionary in `source/tdis/podio_model/`; edit the YAML, not generated
headers, when changing the model. Reconfigure, rebuild and install after a model
change.

The current version is **102**. This is a TDIS data-model version, not a ROOT
version or command-line flag. [Runtime flags](flags.md) change settings such as
Bz and hit covariance. Two runs can have schema 102 but different resolutions
or settings; the performance runner records those in `run.json`.

See [architecture](architecture.md) for the active reconstruction chain,
[CSV format](csv.md) for exported columns, and [performance workflow](tracking-performance.md)
for analysis commands and selection definitions.

## File and collection layout

`-ptdis:output=PREFIX` writes `PREFIX.tdisedm.root`, containing the `events`
tree and PODIO metadata. `podio_metadata.events___CollectionTypeInfo` records
collection names, numeric IDs and schema versions. The default collection list
is in [PodioWriteProcessor.hpp](../source/tdis/io/PodioWriteProcessor.hpp).
An empty collection is valid, including when no seed or fit was produced.

| Collection | Object type | Contents and links |
| --- | --- | --- |
| `EventInfo` | `EventInfo` | Event/run identity and timeslice placeholder |
| `DigitizedMtpcMcTracks` | `DigitizedMtpcMcTrack` | Input truth momentum, angles, vertex z; `hits` links to input hits |
| `DigitizedMtpcMcHits` | `DigitizedMtpcMcHit` | Input pad/time/ADC/drift data and optional true XYZ |
| `TrackerHits` | `TrackerHit` | Global position, local error model and `rawHit` relation |
| `Measurements2D` | `Measurement2D` | Cylinder-local measurement, covariance, `surface`, `hits` and `weights` |
| `TruthTrackParameters` | `TrackParameters` | Initial truth-assisted parameters (`type=-1`) |
| `TruthTrackSeeds` | `TrackSeed` | `perigee` point; `initParams`, `mcTrack`, `hits` and `measurements` relations |
| `FittedTrajectories` | `Trajectory` | KF counts/χ² vectors; `seed` and `trackParameters` relations |
| `FittedTrackParameters` | `TrackParameters` | Fitted perigee and retained cylinder reference states |
| `FittedTracks` | `Track` | Primary Cartesian fitted state; `trajectory` and accepted `measurements` relations |

The YAML also defines types such as `TimesliceInfo`, `TrackSegment` and supporting
surface/point records. Their definition does not mean they are produced by the
current tracking chain.

PODIO relations identify objects by collection ID and index **within an event**.
Indices are not globally unique. For analysis, follow:

```text
FittedTracks.trajectory → FittedTrajectories.seed
  → TruthTrackSeeds.mcTrack → DigitizedMtpcMcTracks

FittedTracks.measurements → Measurements2D.hits
  → TrackerHits.rawHit → DigitizedMtpcMcHits
```

Use `(RunNumber, EventNumber)` to join events; do not assume ROOT entry order
matches CSV order. Current `EventInfo` stores the JANA counter, hard-coded run 22
and timeslice 0. The original text `Event N` label is not preserved. Skipped
records can leave gaps in the counter, and separate runs can reuse identifiers;
retain the input filename/run manifest when combining files.

## Input and reconstructed hit fields

| Object | Fields | Stored meaning / units |
| --- | --- | --- |
| `DigitizedMtpcMcTrack` | `momentum`, `theta`, `phi`, `vertexZ` | GeV, rad, rad, mm; input degrees/metres are converted by the reader |
| `DigitizedMtpcMcHit` | `time`, `adc` | Pad arrival time [ns], input ADC amplitude |
| `DigitizedMtpcMcHit` | `ring`, `pad`, `plane` | Zero-based indices; 21 rings, 122 pads per ring, 10 readout planes |
| `DigitizedMtpcMcHit` | `zToGem`, `padCenterX`, `padCenterY`, `truePosition` | Lengths [mm]; missing true XYZ is NaN |
| `TrackerHit` | `position` | Global XYZ [mm] |
| `TrackerHit` | `positionError` | **Local** cylinder variances [mm²], detailed below |
| `TrackerHit` | `time`, `timeError` | Arrival time and its standard deviation [ns] |
| `TrackerHit` | `cellID` | `1000000 × plane + 1000 × ring + pad`; identifies a pad, not a unique hit |
| `TrackerHit` | `edep`, `edepError` | Currently input ADC and zero; not calibrated energy despite the legacy GeV label |
| `Measurement2D` | `surface`, `loc`, `time` | Cylinder geometry ID, local `(Rφ,z)` [mm], arrival time [ns] |
| `Measurement2D` | `covariance`, `weights`, `hits` | Local covariance, currently one weight of 1, and one linked hit |

Current pad centres use `phi = (pad + 0.5 × odd_ring) × 2π/122`, increasing
counterclockwise in XY. Some legacy input/YAML comments still say clockwise;
the producer and [geometry helper](../source/tdis/geometry/TdisGeometryHelper.hpp)
implement the centre convention above. Distinct hits may share a `cellID` and
have different arrival times.

For a cylinder, `u = Rφ` and `v = z`. Global `position` does not imply a global
XYZ covariance:

```text
TrackerHit.positionError: xx = Var(u), yy = Var(z), zz = 0 (unused)
Measurement2D.covariance: basis [u, z, time]
  xx = Var(u), yy = Var(z), zz = Var(time); xy = xz = yz = 0 currently
Var(u) = (R × 2π/122)² / 12
sigma_z = (5 cm / 1000 ns) × 10 ns = 0.5 mm
Var(z) = 0.25 mm² by default; hit timeError = 10 ns; Var(time) = 100 ns²
```

These are variances, except the explicitly named standard deviation `timeError`.
The KF consumes only the spatial 2×2 block. Stored drift time is not a separate
particle-flight-time measurement. Zero z/time cross terms are a storage
approximation; a future joint timing fit must model their correlation and the
additional diffusion/calibration errors. Hit-error defaults are configurable
where listed in [flags](flags.md#hit-resolution), without a schema change.

## Bound parameters and output references

`TrackParameters` contains `type`, `surface`, `loc`, `phi`, `theta`, `qOverP`,
`time`, `pdg` and `covariance`. The covariance basis is **phi before theta**,
regardless of the order of their scalar declarations in the YAML:

```text
bound vector = [loc0, loc1, phi, theta, q/p, time]
units        = [mm,   mm,   rad, rad,   e/GeV, ns]
```

| Record | `type` | `surface` | `loc.a`, `loc.b` |
| --- | ---: | --- | --- |
| Initial seed parameters | -1 | Default 0; fitter constructs the origin beamline perigee | Signed d0, z0 [mm] |
| Fitted perigee parameters | 0 | 0: z-axis through the origin | Signed d0, z0 [mm] |
| Fitted cylinder parameters | 1 | Nonzero ACTS geometry ID | Rφ, z [mm] |

`d0 = loc.a` is the signed radial impact parameter, not `sqrt(x²+y²)`.
The perigee is closest approach to the entire z-axis, not necessarily the point
`(0,0,0)` and not an independently reconstructed interaction vertex. Under the
current input model the truth vertex lies on that axis, so truth d0 is zero.

On successful inward extrapolation, a trajectory links to the perigee first
and retained cylinder second. On failure, it links only to the cylinder.
`FittedTracks.type=0` means perigee reached; `type=1` means cylinder fallback.
`Trajectory.type=1` instead means a fit was exported; it is a different flag.
Fits that fail before export have no fitted track record, rather than a stored
`type=0` failure trajectory. Always check the collection and field together.

The seed's `TrackParameters.pdg` is currently left at its default value.
Fitting explicitly assumes a proton; fitted parameter and track `pdg` values
are 2212. The seed assumes positive charge, while fitted `qOverP` and
`Track.charge` retain the fitted sign. The text input supplies neither species
nor charge, so these hypotheses are not independent truth labels.

Seed time is initialized from the earliest input-hit arrival time. Its transport
uses the proton hypothesis, but the resulting reference time is not a calibrated
interaction/flight time; the current fit has no independent timing measurement.

## Cartesian track, covariance and fit diagnostics

`FittedTracks.position`, `momentum`, `time`, `timeError` and
`positionMomentumCovariance` describe the **same primary reference**: perigee
on success, cylinder on fallback. Units are mm, GeV and ns. `timeError` is a
standard deviation. The position/momentum covariance uses:

```text
Cartesian vector = [x, y, z, px, py, pz]
units            = [mm,mm,mm,GeV,GeV,GeV]
C_cartesian = J × C_bound × transpose(J)
```

The [output helper](../source/tdis/tracking/FittedTrackOutput.h) uses the
surface-specific bound-to-free position Jacobian and
`d(p_i)/d(q/p) = -p_i/(q/p)`, preserving correlations and the charge sign.
ACTS time-unit factors are applied at the conversion boundary; saved times
and time covariance entries use ns.

Both 6×6 covariances use `Cov6f`, with 21 packed floats. For zero-based indices:

```text
i = min(row, column); j = max(row, column)
packed_index = j × (j + 1) / 2 + i
sequence = C00, C01, C11, C02, C12, C22, ...
Cov6f(i,j) accesses the symmetric element directly.
```

Each covariance element has the product of its two coordinate units; angles
and time cannot be treated as additional mm coordinates.

`Track.chi2` is the sum over accepted measurement-state χ² values. The current
legacy `Track.ndf` counts measured coordinates (`2 × nMeasurements`), so
`chi2/ndf` is not conventional reduced χ². `Trajectory` stores `nStates`,
`nMeasurements`, `nOutliers`, `nHoles`, `nSharedHits`, `measurementChi2` and
`outlierChi2`. Unselected candidates are not rejected outlier states.
`Track.measurements` links accepted measurements; trajectory
`measurements_deprecated` mirrors them and `outliers_deprecated` links outliers.
The `Track.tracks` segment-combination relation is not populated by this fitter.

Full predicted/filtered/smoothed histories are not saved in these collections.
`track_states.csv` contains reference states only; its unavailable path length
and state χ² values are NaN. Optional `diagnostic_event` logging provides selected
per-layer flags, residuals, innovation χ² and q/p values.

## Versions and reading files

| Version | Convention introduced |
| --- | --- |
| 100 | Times in ns; MC, seed and fitted angles in radians |
| 101 | TrackerHit covariance interpreted in local cylinder coordinates |
| 102 | Explicit fitted perigee/cylinder reference and fallback conventions |

Versions 101/102 changed interpretation without changing the field layout.
The timing-derived 0.25 mm² default does not change the schema version.
Model layout changes require a major-version change under the YAML's
`100 × major + minor` convention; semantic changes must also be documented and
versioned. Keep producer, schema, generated classes, readers and tests consistent.

The performance analyzer checks collection IDs and requires schema **102**;
older files are rejected rather than interpreting cylinder `loc.a` as DCA.
PODIO-aware reading uses generated dictionaries. Split numeric ROOT leaves can
also be read with Uproot, or PyROOT with missing-class warnings; the maintained
analyzer uses numeric leaves plus the standard PODIO metadata dictionary.
Reading without TDIS dictionaries does not remove the need to check units,
relations and schema metadata. See the [performance workflow](tracking-performance.md#analyze-existing-output).
