# Tracking performance

From the workspace containing `go_tdis` and `tdis/`, enter the image:

```bash
./go_tdis
cd /worksim
```

Plot an existing schema-102 ROOT file:

Schema 102 is the current TDIS output data-model version, defined in
`source/tdis/edm.yaml`. It identifies fitted perigee parameters so the plotter can
interpret `loc.a` as signed radial DCA.

```bash
python3 tdis/python/plot_tracking_performance.py PATH/TO/reco.tdisedm.root -o output/REPORT
```

Replace `PATH/TO/reco.tdisedm.root` with your reconstruction output ROOT file.
`output/REPORT` is your chosen new report directory. Omit `-o` to create a unique report folder beside the ROOT file.

Minimal Bash example (requires a built/installed TDIS), inside the same image:

```bash
# Run reconstruction only; add --analyze to generate the performance report.
bash tdis/python/run_tracking_performance.sh \
  EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt -n 100

bash tdis/python/run_tracking_performance.sh \
  EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt -n 100 --analyze
```

The Bash script defaults to 100 events, one thread, signed Bz = −4.7 T,
RNG seed 42 and 10% seed momentum smearing. It reconstructs existing digitized
events; the input simulation/digitization must already exist. Hit positions use
finite truth XYZ when present, otherwise pad centres and drift z.
Each invocation creates a new `tdis/tmp/performance-*` directory; use
`-o NEW_DIRECTORY` to choose one. It writes ROOT/CSV output, `reconstruction.log`
with the command, and a small `run.json` with reconstruction settings. With
`--analyze`, it also writes `analysis.log` and the `performance/` report.
To analyze a reconstruction-only run later, use the plotting command above.

The existing Python runner supports more configuration options and can launch
Podman directly from the host. To run events and plot automatically with it:

```bash
python3 tdis/python/run_tracking_performance.py \
  EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt \
  --local -n 1000 --hit-position digitized
```

The `.txt` path is the digitized event input file; replace it with another input
file as needed. The runner creates the output ROOT file and analyzes it.
Default event count is 100. The runner prints the output directory.
Reports include efficiency, dp/p, theta, phi, signed DCAr, phi dependence, PDFs/PNGs and Markdown/CSV/JSON summaries.

See [workflow details](../docs/tracking-performance.md) and [validation results](../docs/tracking-accuracy-study.md).

To plot a fitted single event, inside the same image:

```bash
RUN=tdis/tmp/kf-hit-study-20261023/validated-primary-chi2-25
python3 tdis/python/plot_event_display.py "$RUN/reco.tdisedm.root" \
  --event 7 -o tdis/tmp/fitted-event-7
```

`--event` selects the processed JANA counter. The new output directory contains
an overview, one populated plane per PDF page, matching PNGs and `display.json`.
This script contains the event reader and renderer and only generates the
event display. Run `plot_tracking_performance.py` separately for the full
performance report; it has no event-selection option. All candidates retain
TDC colors; green plus
and red cross markers identify used measurements and selected outliers. The
curve propagates the fitted reference in uniform Bz from the sibling `run.json`;
use `--bz SIGNED_TESLA` only when a field override is needed. The solid inward
extension reaches the fitted beamline perigee; it is not constrained through
the origin. Each plane highlights the trajectory's projected candidate-z
range plus a 2 mm display margin.

To retain KF state diagnostics in JSON, rebuild/install the current fitter and run
with the existing `--diagnostic-event N` (or `-2` for all events). The plotter
automatically reads versioned `KF_POINT` records from the sibling
`reconstruction.log` and validates them against the ROOT fit. Predicted,
filtered and smoothed parameters are retained in `display.json`; state markers
are omitted from the plots. Red outlier crosses are drawn last, above the TDC
points and green used markers. The reusable `draw_pad_pattern()` function in
`plot_event_display.py` draws the full staggered pad layout without any hits.
Older ROOT files work without diagnostic records.
See [design and validation](../docs/track-event-display-plan.md).

For readout plots, use the raw digitized text file directly on the host
(NumPy and Matplotlib required). No reconstruction, CSV, ROOT, manifest or
KF log is needed:

```bash
INPUT=EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt
python3 tdis/python/plot_kf_diagnostics.py "$INPUT" -o tdis/tmp/raw-event-0 --event 0
# Advance to the next event with a new output directory:
python3 tdis/python/plot_kf_diagnostics.py "$INPUT" -o tdis/tmp/raw-event-1 --event 1
# Overlay an inclusive event-ID range on each plane:
python3 tdis/python/plot_kf_diagnostics.py "$INPUT" -o tdis/tmp/raw-overlay-0-49 --event-range 0 49
```

The default displays one event (the first raw record); `--event N` selects
the original `Event N` label in the text file. Both six-column and nine-column
hit formats are supported. XY uses pad centres, and z uses plane position plus
the signed input zToGem (metres converted to mm), even when truth XYZ is present.
Each `event_NNNNNN.pdf` contains only planes with hits,
one plane per page, with matching `event_NNNNNN_plane_PP.png` files. The z/time
projections on each page use that same plane's hits.

An event range writes `events_000000_000049.pdf`, again one populated plane
per page, with all selected events overlaid. Hit colors show input TDC/arrival
time in ns with one shared scale across the selected events and planes;
the z/time projections preserve pulses that overlap in XY. `display.json`
records selected original event IDs, input lines, counts, units and color limits.
Raw records with zero/one hit remain available; empty events produce no plane pages.

For an explicit batch, `--first-events 50` exports separate PDFs for the first
50 raw records. Choose a new output directory each time.

The existing KF audit remains available with `--processed` and a reconstruction
prefix. That mode requires a run made with `--diagnostic-event -2` and uses JANA
event IDs, CSVs/logs and the run manifest. It can also overlay accepted/outlier
markers or run the original candidate/residual checks without displays:

```bash
python3 tdis/python/plot_kf_diagnostics.py RUN/reco --processed --first-events 0 -o RUN/kf-audit
```

The KF figure has rphi plots in row 1, z plots in row 2, and χ²/count plots
in row 3. Its count panel compares measurement candidates and measurements
used per fitted track; `track_measurement_counts.csv` contains those totals.
Here `rphi = Rφ`, and "residual predicted" means measured minus predicted.

See the [hit-level study](../docs/kf-hit-diagnostics-study.md) for results and limits.
