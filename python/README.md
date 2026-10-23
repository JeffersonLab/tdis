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

To run events and plot automatically (requires a built/installed TDIS):

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
