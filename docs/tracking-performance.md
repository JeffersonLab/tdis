# Tracking performance workflow

Run this from the workspace containing `tdis/` on the host:

```bash
python3 tdis/python/run_tracking_performance.py \
  EPC-4p7T-xyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt
```

Only two scripts are needed: `run_tracking_performance.py` runs reconstruction
and analysis; `plot_tracking_performance.py` makes the complete report from an
existing ROOT file, including phi diagnostics. The analyzer is self-contained.

The default is **100 events**, signed Bz = **−4.7 T** (magnitude 4.7 T), one reconstruction thread, and
finite truth XYZ where available, otherwise pad-centre XY and drift z. The
workflow runs reconstruction and analysis in `eicdev/tdis-pre:latest`, using the
existing installed executable at `tdis/build/install/bin/tdis`. Build and install
the current schema-102 perigee-output code first. No Python package installation
is required: the image supplies PyROOT, PODIO metadata support and NumPy.

```bash
# Explicit truth-hit study, with a chosen event count and new output directory.
python3 tdis/python/run_tracking_performance.py \
  EPC-4p7T-xyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt \
  -n 1000 --hit-position truth --bz -4.7 -o output/truth-performance

# Digitized pad-centre/drift-z study, including when input also has truth XYZ.
python3 tdis/python/run_tracking_performance.py \
  EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt \
  -n 100 --hit-position digitized

# Already inside ./go_tdis: run directly without starting another container.
python3 tdis/python/run_tracking_performance.py \
  EPC-4p7T-xyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt --local -n 100
```

`--hit-position truth` enables the existing truth-position setting; individual
hits with non-finite truth XYZ still fall back to digitized positions. `auto`
uses that same reconstruction default. `--hit-position digitized` forces the
digitized path. `--executable PATH` selects another installed executable;
`--image IMAGE` selects a compatible image.

The host runner preserves the build-time `/worksim` mount needed by ROOT's
generated dictionaries. Input and workspace mounts are read-only, while the new
output directory is writable. Explicit output directories must not exist; the
default is a unique `tdis/tmp/performance-*` directory. Earlier data are preserved.

The signed field matches the measured EPC bending under the existing positive-charge
seed convention. The text does not independently specify charge or Bz sign; verify
that convention for other samples. Positive Bz remains available through `--bz`.

The default local hit z resolution is σz = 0.5 mm (variance 0.25 mm²), from
drift velocity 5 cm / 1000 ns and timing resolution σt = 10 ns. The latter is
interpreted as a standard deviation. This is the timing contribution; additional
diffusion and calibration uncertainties are not included. Both truth-position and
digitized-position modes use this covariance, without adding position smearing.
The stored hit time error is 10 ns and measurement time variance is 100 ns²;
the KF continues to use spatial (Rφ, z) measurements only.

The seed has 10% momentum smearing by default. `--seed 42` fixes the RNG sequence;
`--momentum-smear 0` disables smearing. Seed q/p variance now follows that
fractional uncertainty: `variance(q/p) = (smear × q/p_seed)²`. Set
`--qop-variance VALUE` to study a fixed absolute variance; `-1` selects the derived
model. For noiseless validation with unconstrained curvature, use
`--momentum-smear 0 --qop-variance 0.1`. A zero-smear seed with derived variance
would instead encode exactly known momentum.

`--diagnostic-event 99` logs per-layer measurement IDs, innovation/state χ²,
measurement/outlier/hole flags and predicted/filtered/smoothed q/p for that event.
No χ² rejection is enabled by default. The legacy `ndf` field still counts measured
coordinates; do not interpret its ratio as conventional reduced χ².

See the [iterative accuracy investigation](tracking-accuracy-study.md) for the
1,000-event comparisons, rotational-invariance checks and remaining limitations.

## Outputs

The runner prints the output path and a concise numeric report. Each run contains:

- `reco.tdisedm.root` and the reconstruction CSV files.
- `reconstruction.log`, `analysis.log`, and `run.json` with commands, requested
  count, field, hit-position setting, input/executable/script hashes, Git state,
  image identity and exit codes. The runner uses fixed RNG seed 42 by default.
- `performance/performance.pdf`: source/configuration cover, four residual plots,
  and efficiency versus truth momentum, theta and phi.
- `performance/resolutions.png` and `performance/efficiency.png`.
- `performance/phi_dependence.pdf/png`, `phi_bins.csv`, and `phi_summary.json`:
  phi-binned mean/central-68% width, bootstrap width errors and tests of constant
  behavior. First/second phi harmonics also control for momentum/theta quartile
  strata, with HC3 covariance. These diagnostic regressions do not alter tracks.
  Paired rotations test the algorithm directly, holding each event fixed.
- `performance/summary.md` and `summary.json`: counts, efficiency intervals,
  residual biases and widths, Gaussian fit status/quality, and limitations.
- `performance/tracks.csv`: one row per truth particle, including failed fits,
  seed/fit/perigee flags, matched object indices and residuals.
- `performance/efficiency.csv`: counts and Wilson intervals for each kinematic bin.

## Definitions and selection

The denominator is **every truth track in the processed ROOT events**, including
tracks without seeds or successful fits. Requested events and processed events
are reported separately. An early input/source stop does not turn unread events
into fit failures. The input reader accepts hit-bearing records, so this measures
truth-assisted fitting efficiency for that sample, not detector acceptance or
independent pattern-recognition efficiency.

An exported fit counts toward fit efficiency. A finite perigee with nonzero q/p
counts toward perigee efficiency and the residual distributions. Cylinder
fallbacks count toward fit efficiency but are excluded from perigee residuals.
No hit-count, chi2, residual or charge-sign cut is applied. Each truth particle
counts once; duplicate fits are reported and the first valid perigee in collection
order is used. Matching follows the PODIO track → trajectory → seed → MC relations,
checking collection IDs and indices; entry order alone is not used for matching.
Older schemas are rejected because a cylinder's loc.a is not radial DCA.

```text
dp/p [%] = 100 × (abs(1/qOverP_fit) − p_truth) / p_truth
Δtheta [mrad] = 1000 × (theta_fit − theta_truth)
Δphi [mrad] = 1000 × atan2(sin(phi_fit − phi_truth), cos(phi_fit − phi_truth))
DCAr [mm] = signed perigee loc.a
```

DCAr is signed impact parameter, not sqrt(x²+y²). The text-input model places
the truth vertex on the beam axis and supplies only vertex z, so truth DCAr is
zero. This assumption must be revisited for displaced transverse vertices.
Theta/phi truth angles come from the input header and are stored in radians;
the fit is evaluated at closest approach to the origin z-axis.

Following the ePIC `plot_single_resol.py` example, Gaussian core fits start from
median/MAD estimates and use up to five robust ±3σ clipping iterations. At least
50 entries must remain in the fit window. The final fit uses a binned Poisson
likelihood over that window, with bin integration. Sigma and its fit uncertainty
are reported only when minimization and the covariance estimate succeed. Small
or degenerate samples explicitly have no Gaussian result. Gaussian χ²/ndf and
histogram shape help assess whether a Gaussian core is an adequate description.

The report also retains the full-sample mean, sample standard deviation (N−1)
and central 68% half-width `(q84 − q16)/2`. Tails are included in those statistics;
the histogram displays ±4 robust widths and reports the core count. Inclusive
widths mix the input momentum/angle spectrum and should not be interpreted as
resolution at a fixed momentum. Use `tracks.csv` for further kinematic binning.
Efficiencies use 68.27% Wilson intervals, including nonzero uncertainty at 0%/100%.

## Analyze existing output

Inside `./go_tdis`, analyze a schema-102 file without rerunning reconstruction:

```bash
python3 tdis/python/plot_tracking_performance.py \
  tdis/tmp/YOUR_RUN/reco.tdisedm.root -o tdis/tmp/YOUR_RUN/new-report
```

The input ROOT file is opened read-only. The report directory must be new. A
neighboring `run.json` supplies provenance when present. The analyzer reads numeric
TDIS leaves without loading TDIS dictionaries; standard PODIO dictionaries are
used only for collection IDs and schema metadata.

The completed numerical, ROOT-association and rotation checks are recorded in
the [accuracy study](tracking-accuracy-study.md). Temporary validation scripts
and generated test files were moved to the ignored `tmp/retained/2026-10-23/`
holding area. Source regression tests for candidate selection, perigee transport
and pad geometry remain available through `ctest --test-dir build --output-on-failure`
inside the image.
