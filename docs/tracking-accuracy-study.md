# Tracking accuracy: iterative 1,000-event investigation

Updated 2026-10-23 10:17 PDT.

The later [hit-level KF study](kf-hit-diagnostics-study.md) adds first-50-event
readout displays, a configurable innovation gate and a last-layer completion
fix. The tables below retain this earlier uncut baseline.

The requested dp/p < 40% target is met for both truth-XYZ and digitized reconstruction, including the full-sample standard deviation and RMSE. No residual or chi² rejection was added. Matched event rotations show no detectable algorithmic phi dependence at the tested precision.

These remain truth-assisted fits with a 10% momentum seed. The correctly propagated seed uncertainty matters at high momentum, where the pad measurements alone constrain curvature weakly. These numbers are not independent detector-only momentum resolutions.

## Drift-time hit resolution update — 2026-10-23

The supplied drift velocity is 5 cm / 1000 ns = 0.05 mm/ns. Interpreting the
10 ns time resolution as one standard deviation gives:

```text
sigma_z = abs(v_drift) × sigma_t = 0.05 mm/ns × 10 ns = 0.5 mm
Var(z) = sigma_z² = 0.25 mm²
Var(time) = (10 ns)² = 100 ns²
```

The default `TruthTracksSeedsHitsFactory:acts:hit:cov:z` is now 0.25 mm²,
replacing the earlier 100/12 mm² placeholder. Both TrackerHit local z variance
and Measurement2D z variance use it; hit timeError is now 10 ns and the stored
measurement time variance is 100 ns². The KF uses only (Rφ, z), so drift time
is not fitted as a separate independent measurement. Input positions are
unchanged, including the truth-position mode. The existing variance override
remains available.

This is the timing contribution alone. Diffusion, drift-speed and time-zero
calibration uncertainties, and charge-sharing correlations still need a model.
The earlier investigation tables below use the old z placeholder and remain
as the baseline for comparison.

Validation uses the same file-0 inputs, fixed RNG 42, 10% seed momentum smearing and signed Bz = −4.7 T. Each mode processes 1,000 events with no added cuts.

| Mode | Fits / valid perigees | dp/p bias [%] | dp/p sample SD [%] | Central 68% [%] | Gaussian core σ [%] |
| --- | ---: | ---: | ---: | ---: | ---: |
| Truth XYZ | 993 / 993 | 4.61 | 12.72 | 8.93 | 9.18 |
| Digitized | 993 / 993 | 6.33 | 18.89 | 9.71 | 9.76 |

| Mode | Residual | Bias | Sample SD | Central 68% half-width | Gaussian core σ |
| --- | --- | ---: | ---: | ---: | ---: |
| Truth XYZ | Δtheta [mrad] | -0.7658 | 7.954 | 4.842 | 4.943 |
| Truth XYZ | Δphi [mrad] | -5.866 | 27.4 | 13.78 | 13.74 |
| Truth XYZ | DCAr [mm] | 0.1258 | 0.6024 | 0.3025 | 0.2479 |
| Digitized | Δtheta [mrad] | -0.6858 | 7.877 | 5.074 | 5.039 |
| Digitized | Δphi [mrad] | -0.8129 | 28.21 | 14.65 | 14.57 |
| Digitized | DCAr [mm] | 0.2753 | 0.6605 | 0.4836 | 0.5097 |

Efficiency remains 99.3% in both modes. The full dp/p SD remains below 40%, but grows from 9.88% → 12.72% (truth) and 10.83% → 18.89% (digitized). The central widths and Gaussian cores change much less: the narrower assigned hit error increases the influence of z and leads to larger fitted tails in these samples. A smaller assigned measurement error does not by itself establish a better fitted resolution; residual/model consistency, radial-bin projection and candidate/cluster correlations still need study. No covariance inflation or rejection cut was introduced to compensate.

Momentum/theta-controlled phi-harmonic p-values are 0.385/0.278 for dp/p (truth/digitized); all four residual metrics have p > 0.19 in both modes. The earlier paired rotation results below belong to the original covariance study and were not rerun for this update.

Rebuilt and installed the executable using the same image ID as the baseline; 3/3 CTests pass. Direct ROOT checks cover 114,023 truth-position hits and 114,051 digitized-position hits over 1,000 events per mode (including six empty hit collections each): every stored z variance is 0.25 mm², hit time error is 10 ns and measurement time variance is 100 ns². Hit positions, times, cell IDs, azimuthal variances and measurement local coordinates match the earlier files exactly.

Generated checks are kept out of the active tree in `tmp/retained/2026-10-23/drift-resolution-171154/`, with `truth/`, `digitized/`, `build-installed.log` and `validation.log`. The two existing analysis scripts were sufficient; no new scripts were added. This section records the hit-resolution change and its validation.

## Plan and completed checks

| Order | Inspection | Evidence / result |
| --- | --- | --- |
| 1 | Freeze baseline and metric | 1,000 processed events, all-truth denominator, zero-smear control first, then fixed RNG 42 and 10% smearing. Report bias, sample SD, RMSE, central 68% width and Gaussian core. |
| 2 | Input geometry, field and charge | Independent circles through unique raw truth points: all 1,000 bend with q × Bz < 0 in stored XYZ. Existing positive-charge seeds therefore require negative Bz. |
| 3 | Field-only comparison | With all else fixed, changing +4.7 T to −4.7 T reduces truth dp/p SD from 2,904.6% to 7.52% in the zero-smear control. |
| 4 | Seed covariance and weak curvature | The old absolute q/p variance 0.1 assigned >100% relative uncertainty to some high-p seeds despite 10% actual smearing. Correct variance is (f × q/p_seed)² to first order. Digitized SD falls from 250.1% to 10.64% in the paired 10%-smear comparison, before the pad-centre correction. |
| 5 | Pad geometry | Across 114,023 truth-associated hit rows, pad-index centre residuals average −0.100/+0.033 mrad on even/odd rings. The old extra half pitch shifted centres by 25.75 mrad. Removed that shift, retained odd-ring stagger, and tested all 21 × 122 centres. |
| 6 | Analytic helix and transport | Independent analytic Lorentz-force inputs, 1,000 events each for both field signs: 1,000/1,000 fits/perigees. Unconstrained q/p covariance retained; numerical dp/p SD is 0.0283%. |
| 7 | KF chi² and flags | Optional event-99 layer trace: independently computed innovation chi² matches ACTS state chi²; state sums match the exported total; candidate IDs and measurement/outlier/hole flags reconcile. No outlier finder or chi² rejection is configured. |
| 8 | Phi symmetry | Matched 1,000-event rotations at 50.16° and 180° in both modes, plus arbitrary 37° truth-XYZ rotation. Status is unchanged; all per-track residual differences pass numerical tolerances. |
| 9 | Independent sample | Repeat on the first 1,000 processed events of file 10000 in each mode; SD remains 10.43% / 11.32%. |
| 10 | Regression and provenance | 3 CTests and 6 Python tests pass. Commands, seed, field, hashes and image ID are recorded; all development outputs and source data are preserved. |

## Fixes and conventions

- Default signed Bz is now −4.7 T, while its magnitude remains 4.7 T. This follows the existing +1 charge convention and the measured bending. The text input does not independently identify charge or the signed field: upstream confirmation remains necessary before assigning physical charge labels. Positive Bz is still configurable and passes the analytic positive-field control.
- Seed q/p covariance defaults to the first-order transformation of the configured fractional momentum smearing. A nonnegative absolute variance override remains available. Zero smearing with the derived variance encodes exactly known momentum; analytic recovery tests explicitly use variance 0.1 instead so momentum is free to move.
- Fixed RNG seed 42 is the workflow default, with one thread; the factory retains random-device behavior when its random_seed parameter is 0.
- Pad indices label angular centres: phi = (pad + 0.5 × odd_ring) × 2π/122. The extra half-pitch offset was incorrect for these inputs. Digitized mean delta-phi improves from +16.80 mrad to −2.79 mrad in the paired seed/covariance comparison.
- The output track proxy now retains the proton hypothesis actually used for fitting; the ACTS vector container previously left the output labelled as a pion and used that hypothesis for the inward time extrapolation.

## Baseline results before the drift-time resolution update

Every row below processes 1,000 events. Sample SD uses N−1; RMSE includes bias. Percentages are absolute percentage points of (p_fit − p_truth)/p_truth. Failed/seedless events remain in the efficiency denominator; all valid perigees enter the residual statistics.

| Input / mode | Fits / perigees | Bias [%] | SD [%] | RMSE [%] | Central 68% [%] | Gaussian core σ [%] |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| File 0 / truth XYZ | 993 / 993 | 3.80 | 9.88 | 10.57 | 8.92 | 9.17 ± 0.23 |
| File 0 / digitized | 993 / 993 | 5.38 | 10.83 | 12.09 | 9.74 | 9.63 ± 0.25 |
| File 10000 / truth XYZ | 992 / 991 | 3.61 | 10.43 | 11.03 | 8.98 | 9.38 ± 0.24 |
| File 10000 / digitized | 991 / 991 | 4.90 | 11.32 | 12.33 | 9.98 | 9.87 ± 0.25 |

The field correction alone fixes the gross truth-XYZ failure. The covariance correction removes the large digitized momentum tails without hiding bad tracks. The pad correction fixes a separate angular-origin error; it does not improve every scalar metric monotonically. Remaining few-percent bias is reported rather than subtracted.

Primary-file angle and signed-impact-parameter results (993 valid perigees in each mode):

| Mode | Residual | Bias | Sample SD | Central 68% half-width | Gaussian core σ |
| --- | --- | ---: | ---: | ---: | ---: |
| Truth XYZ | Δtheta [mrad] | -0.5856 | 5.927 | 4.469 | 4.335 |
| Truth XYZ | Δphi [mrad] | -8.352 | 17.02 | 13.21 | 13.14 |
| Truth XYZ | DCAr [mm] | 0.1757 | 0.3194 | 0.2786 | 0.1619 |
| Digitized | Δtheta [mrad] | -0.3325 | 4.825 | 3.334 | 3.214 |
| Digitized | Δphi [mrad] | -2.786 | 16.95 | 13.96 | 14.51 |
| Digitized | DCAr [mm] | 0.3327 | 0.501 | 0.4755 | 0.503 |

The truth-XYZ DCAr distribution is strongly non-Gaussian (Gaussian χ²/ndf = 5.30); its full SD and central width better describe the inclusive spread than the narrow Gaussian core.

## Phi checks

For each paired rotation, momentum/theta, z, drift values and RNG sequence stay fixed. Pad-pitch rotations update cell indices consistently; the continuous truth-only test rotates XYZ/header phi while retaining cell labels, which the truth-position path does not use for XY measurements.

| Mode | Rotation | Matched perigees | Maximum change in dp/p [percentage points] |
| --- | ---: | ---: | ---: |
| Truth XYZ | 50.16° | 993 | 0.001326 |
| Digitized | 50.16° | 993 | 0.001008 |
| Truth XYZ | 180.00° | 993 | 0.000071 |
| Digitized | 180.00° | 993 | 0.000083 |
| Truth XYZ | 37° continuous | 993 | 0.001500 |

All five rotations preserve fit/perigee status. Other residual tolerances are 0.02 mrad for theta, 0.05 mrad for phi, and 0.001 mm for DCAr; all pass. The momentum tolerance is 0.02 percentage points, well below the measured resolution. The maximum observed momentum change is 0.00150 percentage points.

Phi-bin widths with bootstrap errors are consistent with constant behavior. First/second Fourier-mode tests controlling for momentum/theta quartile strata give dp/p p-values 0.300 / 0.144 on file 0 and 0.919 / 0.946 on the independent sample (truth / digitized). None of the four residuals shows a significant controlled harmonic at 5% in these samples.

The unconditioned digitized mean-bias bin test on file 0 is marginal (p = 0.038), while its width test has p = 0.262. That marginal mean variation is not reproduced in the independent sample (p = 0.984), and matched rotations are invariant. The raw plots remain visible; no phi-dependent correction or selective removal was applied. Finite samples cannot prove an exactly zero physical dependence. Digitized pad-pitch symmetry is distinguished from continuous truth-surface symmetry.

## Chi², measurements and remaining limitations

- The calibrator still selects one minimum-innovation-chi² candidate per cylinder. Unselected candidates remain available; they are not classified as rejected outliers. No cut was introduced to meet the momentum target.
- Final file-0 truth/digitized runs have 0 outliers and 2 total holes each. Independent runs have 0 outliers and 6/5 holes. Hole counters are therefore not universally zero at 1,000 events. They are retained, and none is excluded from the resolution sample.
- Legacy Track.ndf is still the sum of calibrated dimensions (2 × accepted measurements). Its CSV ratio is not a conventional reduced chi²; informative seed priors also affect the appropriate statistical interpretation. No decision in this study uses that ratio as a quality cut.
- One/two-layer fits remain included and can be prior dominated. A detector-only study needs independently estimated seeds, observability/lever-arm requirements and calibrated measurement/cluster errors. The current result is explicitly truth assisted.
- Residual bias remains, along with the radial-bin projection approximation, ignored charge-sharing correlations, constant-field approximation, disabled material effects, hard-coded positive/proton seed convention, and drift-time calibration assumptions. The original z-variance placeholder has since been replaced by the timing-derived value described above; additional diffusion/calibration contributions remain open. Branching/cluster reconstruction and a full tracking-volume/material model remain future work.

## Reproduction

From the workspace containing tdis/:

```bash
python3 tdis/python/run_tracking_performance.py \
  EPC-4p7T-xyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt \
  -n 1000 --seed 42 --diagnostic-event 99

python3 tdis/python/run_tracking_performance.py \
  EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt \
  -n 1000 --seed 42 --hit-position digitized
```

To regenerate only the reports, enter `./go_tdis` and run:

```bash
python3 tdis/python/plot_tracking_performance.py PATH/TO/reco.tdisedm.root \
  -o PATH/TO/NEW_REPORT
```

The two maintained scripts generate efficiency, dp/p, theta, phi, signed DCAr and phi-dependence reports. The default event count remains 100; the study commands above explicitly request 1,000. No separate symmetry module or validation generator is required for normal analysis. See [workflow details](tracking-performance.md) for all options and output definitions.

## Cleanup and validation record

On 2026-10-23, the temporary analytic/rotation generators, standalone symmetry checker, Python validation harnesses and 43 generated run directories were moved out of the active tree into the ignored local holding area `tmp/retained/2026-10-23/`. This follows the workspace's reversible-retention policy; no raw input was moved or rewritten. Each relocation is recorded in `tmp/file_actions.log`.

- This Markdown study is the permanent record of the iterations, metrics, tests and limitations. Generated ROOT/CSV files, plots, one-off scripts and logs are not committed.
- The previous `tmp/accuracy-326kzmx4/` is now `tmp/retained/2026-10-23/tmp/accuracy-326kzmx4/`. Final primary runs are `18-pad-truth` and `19-pad-digitized`, independent runs are `20-holdout-truth` and `21-holdout-digitized`, and rotations are `22-...` through `26-continuous-rotation`. Older 999-event development runs are not acceptance evidence.
- Retired scripts are in the holding area's `python/` directory. Permanent C++ regression tests remain in `source/tests/` for candidate selection, perigee/covariance transport and pad geometry.
- Before cleanup, 3/3 CTests and 6/6 Python checks passed. A fresh 1,000-event digitized reconstruction using only the two maintained scripts reproduced 993 valid perigees, dp/p bias 5.3794% and SD 10.8287%. Reanalysis of the truth-XYZ 1,000-event ROOT file reproduced its results as well. Counts, residual statistics, Gaussian fits, efficiencies and phi-diagnostic JSON were identical to the previous final reports in both modes.
- Cleanup checks are retained under `tmp/retained/2026-10-23/tmp/commit-check-1000/` and `commit-check-truth-report/`; the original inputs and installed build remain in place.

The checks used image ID `730f833444a3b0be3cf48cf6204df006fbb930f636a104ff42ec1af654c3a2a6` and installed executable SHA256 `07cea122e929bf210ab3ba190dae55a688cf86a26a6d06c46b2c1f10a688f25c`. Input hashes and full commands remain in the archived run manifests. Holdout inputs are the corresponding `g4sbsout_EPCEvents_10000.txt` files.

## Focused commits

- `e2b3e32` — Write fitted tracks at beamline perigee.
- `ac930f0` — Match the field sign to EPC bending.
- `5bca46a` — Make seed smearing reproducible.
- `3664441` — Match seed covariance to momentum smearing.
- `1be9a48` — Fix the pad centre angular offset.
- `b49fbbb` — Add event-level KF diagnostics.
- `df19f61` — Add the tracking performance workflow.
- `90fb843` — Ignore local tracking study artifacts.

This summary is committed separately from the implementation and housekeeping. Every commit credits ChatGPT.
