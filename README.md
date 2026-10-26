[![python tests](https://github.com/JeffersonLab/tdis/actions/workflows/pytdis.yml/badge.svg)](https://github.com/JeffersonLab/tdis/actions/workflows/pytdis.yml) 
[![c++ build and test](https://github.com/JeffersonLab/tdis/actions/workflows/linux-build-test.yml/badge.svg)](https://github.com/JeffersonLab/tdis/actions/workflows/linux-build-test.yml) 
[![documentation](https://github.com/JeffersonLab/tdis/actions/workflows/documentation.yml/badge.svg)](https://github.com/JeffersonLab/tdis/actions/workflows/documentation.yml) 

ACTS and JANA2 based package for TDIS multi-TPC chamber tracking reconstruction. 

Follow the [=>MAIN DOCUMENTATION<==](https://jeffersonlab.github.io/tdis/#/)

To run 100 events and plot fit efficiency plus momentum, angle and radial DCA
resolution, see the [tracking performance workflow](docs/tracking-performance.md).

## ROOT output and schema 102

A **schema** defines the saved objects, fields, units and relationships in a
reconstruction file. **102 is the current TDIS data-model version**, declared
in [source/tdis/edm.yaml](source/tdis/edm.yaml). It is not a ROOT software version,
event count or reconstruction setting. PODIO stores this version for each
collection in the ROOT file's `podio_metadata` tree.

Schema 102 distinguishes a fitted beamline perigee (`FittedTracks.type = 0`)
from a cylinder fallback when inward extrapolation fails (`type = 1`).
`FittedTrackParameters` uses the same reference types; perigee surface ID 0
means closest approach to the z-axis through the origin, not a vertex constrained
to (0,0,0). Lengths are stored in mm, angles in radians and times in ns.
Version 101 established the cylinder-local hit covariance convention; 102
adds these reference/status conventions without changing the field layout.

The tracking plotters require schema 102 so they can interpret these fields
correctly. Renaming an older file does not convert its schema. For collection
definitions, units and compatibility details, see [the schema reference](docs/schema.md).
Run settings such as Bz and the chi2 cut are separate and recorded in `run.json`.

## Plot a fitted event

Inside the TDIS image, from `/worksim`:

```bash
python3 tdis/python/plot_event_display.py PATH/TO/reco.tdisedm.root \
  --event 7 -o tdis/tmp/event-7-display
```

Replace the ROOT path and choose a new output directory. `--event` selects the
processed JANA event ID. The directory contains an overview and one populated
plane per PDF page, matching PNGs and `display.json`; this script only generates
the event display. Signed Bz comes from the sibling `run.json`, or an explicit
`--bz` value. See [Python examples](python/README.md) for the full performance
report and independent raw-hit plotting commands.

