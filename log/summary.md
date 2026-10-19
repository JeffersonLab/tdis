# TDIS work summary

## 2026-10-18: Container runtime library lookup

- 2026-10-18 17:04 PDT — Inspected `build/CMakeCache.txt`, `build/install_manifest.txt`, and `build/source/tdis/cmake_install.cmake` to identify the installed-library lookup failure; reproduced it in the local `eicdev/tdis-pre:latest` image (ID `730f833444a3`) and confirmed that adding `/usr/local/lib` to `LD_LIBRARY_PATH` makes `tdis --help` succeed.
- 2026-10-18 17:06 PDT — Updated `source/tdis/CMakeLists.txt` with Linux install paths relative to each binary and library, and documented the workaround and persistent container installation in `docs/install.md`, so the installed executable finds its companion data model libraries.
- 2026-10-18 17:06 PDT — Reconfigured and rebuilt the existing `build/` tree using the dependency image and installed to `/worksim/tdis/build/install` (host: `/work/halla/tdis/shujie/tdis/build/install`), keeping the installation on the mounted filesystem; outputs are recorded in `build/runtime-fix-configure.log`, `build/runtime-fix-build.log`, and `build/runtime-fix-install.log`.
- 2026-10-18 17:07 PDT — Verified `tdis --help`, installed ELF runtime paths, and dependency resolution in a fresh container with a read-only mount; `build/runtime-fix-validation.log` records successful startup and no missing libraries, while event processing and physics outputs were not tested.

Inside a container started with `-v /work/halla/tdis/shujie:/worksim`, use:

```bash
export PATH="/worksim/tdis/build/install/bin:$PATH"
tdis --help
```

Repeat the PATH command in each new container shell. The build cache now uses
`/worksim/tdis/build/install` as its installation prefix instead of `/usr/local`.
No data files were changed or relocated.

## 2026-10-18: One-command container launcher

- 2026-10-18 17:34 PDT — Created executable `../go_tdis` using the existing `eicdev/tdis-pre:latest` image and `build/install/bin/tdis`; it mounts its containing directory at `/worksim`, starts an interactive Bash shell there, and prepends the installed TDIS directory to PATH.
- 2026-10-18 17:34 PDT — Checked `../go_tdis` with `bash -n` and launched it in a terminal; `pwd` returned `/worksim`, `command -v tdis` resolved to `/worksim/tdis/build/install/bin/tdis`, and `tdis --help` succeeded.

From the host, run `/work/halla/tdis/shujie/go_tdis`; type `exit` to leave the container.

## 2026-10-18: Preserve JANA event identity in ROOT output

- 2026-10-18 20:59 PDT — Updated `source/tdis/io/DigitizedDataEventSource.hpp` to copy JANA event/run IDs into PODIO `EventInfo`, keeping timeslice zero and checking the existing integer event-ID range, so ROOT records match the CSV event identity.
- 2026-10-18 20:59 PDT — Rebuilt and installed `build/install/bin/tdis`, then processed three events from `../EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt` with `geometry/g4sbs_mtpc.root`; logs and separate ROOT/CSV outputs are retained in `tmp/event-identity-xt7Zlr/`.
- 2026-10-18 20:59 PDT — Checked the new ROOT event IDs `[0, 1, 2]`, run IDs `[22, 22, 22]`, zero timeslices, and CSV agreement including MC momentum joined by event ID; `tmp/event-identity-xt7Zlr/validation.log` records the checks, while existing fit failures remain and the original output ROOT checksum is unchanged.
- 2026-10-18 20:59 PDT — Added the fix, validation, remaining identity limitations, and a field/unit issue table to the 1A log block of `../workplan.md`; no changes were made to item 1B or to the listed field/unit behavior.

The source still assigns sequential JANA event numbers and hard-codes run 22;
original text `Event N` labels and globally unique IDs across separate jobs are
not supplied by this fix. Existing ROOT files are unchanged and must be
regenerated to obtain the corrected metadata.

## 2026-10-18: Cell IDs and explicit output units

- 2026-10-18 21:26 PDT — Updated `source/tdis/io/DigitizedDataEventSource.hpp`, `tracking/TruthTracksSeedsHitsFactory.cpp`, and `tracking/KalmanFittingFactory.cpp` (the latter two under `source/tdis/`) so cell IDs encode the raw hit's plane/ring/pad, output times and time covariances use ns, and ACTS boundaries convert with `UnitConstants::ns` while preserving the existing physical uncertainty.
- 2026-10-18 21:26 PDT — Updated `source/tdis/edm.yaml` to schema 100 with mm/radian/ns units and the implemented phi-before-theta covariance order, and clarified angle comments in `source/tdis/io/CsvWriteProcessor.hpp`; MC degrees are converted only at text input, and the existing plotting scripts remain unchanged.
- 2026-10-18 21:26 PDT — Rebuilt/installed `build/install/bin/tdis`, ran three events from `../EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt` with `geometry/g4sbs_mtpc.root`, and validated 375 hit records against input and ROOT/CSV outputs; final artifacts and reproducible checks are in `tmp/field-units-radians-uGHeCf/`, with passing results in `validation-final.log`.
- 2026-10-18 21:26 PDT — Updated only the 1A log block in `../workplan.md` with completed fixes, successful-fit validation limits, remaining issues, and the newly observed plane-indexing defect; the original ROOT file checksum and item 1B text were preserved.

`tmp/field-units-XKaxyH/` retains an intermediate run; use the radians-labelled
directory above for the final implementation. No existing outputs were moved
or deleted. The fitter still fails for the three validation events.

## 2026-10-18: Commit reviewed fixes

- 2026-10-18 22:02 PDT — Committed the installation fix as `e917ea8` and prepared the event-ID, cell-ID, unit, schema, and summary changes as a separate commit at the user's request; both messages credit ChatGPT, and generated `tmp/` artifacts are excluded.
- 2026-10-18 22:02 PDT — Checked the diffs and reused the completed build and three-event/375-hit validation; `../workplan.md` and `../go_tdis` remain outside the Git repository and are not included in its commits.
