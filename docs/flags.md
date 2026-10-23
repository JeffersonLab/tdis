## tdis parameters

`tdis` software parameters (i.e. command line flags) that can be set when running TDIS applications.

These configure a run; they do not define the saved [data schema](schema.md).
See [architecture](architecture.md) for which services and algorithms are active.

Parameters in JANA2 applications should be provided using `-p` flag. E.g. if parameter is `acts:log_level` then to set
the parameter from command line one uses 

```bash
tdis -pacts:log_level=debug   # ... other parameters
```


Main TDIS application flags.

- `tdis:LogFormat` - *info* - spdlog pattern string
- `tdis:LogLevel` - *info* - log_level: trace, debug, info, warn, error, critical, off


## ACTS Flags

ACTS (A Common Tracking Software) geometry and conversion flags.

- `acts:geometry` - *g4sbs_mtpc.root* - TGeo filename with geometry for ACTS
- `acts:log_level` - *info* - log_level for acts: trace, debug, info, warn, error, critical, off
- `acts:material_map` - *(empty)* - JSON/CBOR material map file path
- `acts:output_obj` - *(empty)* - Output file name to dump ACTS converted geometry as OBJ
- `acts:output_ply` - *(empty)* - Output file name to dump ACTS converted geometry as PLY
- `acts:round_tgeo_values` - *(empty)* - **Warning: Unused parameter**
- `acts_init:log_level` - *info* - log_level for acts_init: trace, debug, info, warn, error, critical, off

---

## Component-Specific Flags

### I/O Component Flags

- `csv:prefix` - *output* - File name prefix with path for CSV output files
- `io:log_level` - *info* - log_level for io: trace, debug, info, warn, error, critical, off
- `io:tracks_per_event` - *1* - Number of tracks to combine per event

### PODIO Writer Flags

- `podio:output_file` - *podio_output.root* - Podio output file to write to
- `podio:print_collections` - *(empty)* - Comma separated list of collection names to print to screen, e.g. for debugging
- `podio:log_level` - *info* - log_level for PodioWriteProcessor: trace, debug, info, warn, error, critical, off


### Kalman Fitter Flags

- `KalmanFittingFactory:acts_level` - *INFO* - ACTS log level (VERBOSE|DEBUG|INFO|WARNING|ERROR|FATAL)
- `KalmanFittingFactory:bz` - *-4.7* - Signed magnetic field along Z, in Tesla. The magnitude is 4.7 T; negative Bz matches EPC bending with positive-charge seeds. Override with `tdis -pKalmanFittingFactory:bz=VALUE input.txt`. The effective field is printed at startup.
- `KalmanFitterGenerator:InputTags` - *TruthTrackSeeds* - Input collection names
- `KalmanFitterGenerator:loglevel` - *info* - Component log level
- `KalmanFitterGenerator:OutputTags` - *FittedTrajectories,FittedTrackParameters,FittedTracks* - Output collection names
- `pkalmanfittergenerator:acts_level` - *(empty)* - **Warning: Unused parameter**
- `tracking:kf:log_level` - *info* - log_level for tracking:kf: trace, debug, info, warn, error, critical, off

Repeated pad hits remain separate candidate measurements, including different
input times. At each cylinder, the fitter chooses one candidate using the
predicted local position and covariance:

```text
u = Rφ, v = z
residual = candidate(u,v) − predicted(u,v)
S = predicted local covariance + measurement covariance
χ² = residualᵀ S⁻¹ residual
```

The smallest finite χ² wins; exact ties keep input order. This is a greedy
single-track choice, without branching or an additional χ² rejection threshold.
The Rφ residual is periodic. The chosen measurement ID is retained in fitted
track relations and reused during backward filtering. Unchosen candidates remain
in `Measurements2D`; they are not classified as fitted outliers.

This is a 2D position fit: input time is retained but is not an independent
selection coordinate. Different drift times can be distinguished through their
supplied z positions; identical local positions and covariances are tied.
TDIS does not convert TDC to z itself. Seeds are propagated from their perigee to
the innermost candidate cylinder, with covariance transport, before tracking.
This assumes the current near-axis, outward-going seed model; it is not a
multi-turn track finder.

After smoothing, the fitted inner-cylinder state is extrapolated backward to
closest approach to the z-axis through the origin. This uses the same configured
field and transports covariance without geometry navigation or material effects,
with a 10 m path limit. `FittedTracks.type=0` indicates perigee success; `type=1`
indicates extrapolation failure and cylinder fallback. The cylinder bound state
is retained in either case; successful output also stores the perigee first.
See [CSV/reference-state conventions](csv.md#fitted_trackscsv).

The candidate-selection regression test can be run after building with
`ctest --test-dir build -R layer_candidates --output-on-failure`.
Perigee transport and Cartesian covariance tests run with
`ctest --test-dir build -R fitted_output --output-on-failure`.

### Hit resolution

- `TruthTracksSeedsHitsFactory:acts:hit:cov:z`: default 0.25 mm², the local cylinder z variance. With drift velocity 5 cm / 1000 ns = 0.05 mm/ns and timing standard deviation 10 ns, σz = vdrift × σt = 0.5 mm. The same variance is stored in TrackerHit local covariance and Measurement2D. Override this variance directly for other resolutions.

The 10 ns resolution is interpreted as one standard deviation, rather than a
time-bin width. This models the timing contribution alone; diffusion, drift-speed
calibration, time-zero uncertainty and charge-sharing correlations are separate
effects. This covariance setting does not change the input z positions.
TrackerHit timeError is 10 ns and Measurement2D time variance is 100 ns².
The KF uses only the two spatial coordinates; drift time is not added as an
independent third measurement of z.

### Active seed-study controls

The active component prefix is `TruthTracksSeedsHitsFactory`:

- `acts:track_init:random_seed`: 0 uses random_device; a positive integer gives a fixed sequence (the performance runner uses 42 and one thread).
- `acts:track_init:momentum_smear`: fractional momentum uncertainty, default 0.1; 0 disables smearing.
- `acts:seed:cov:qoverp`: default -1 derives variance as `(momentum_smear × q/p_seed)²`; a nonnegative value overrides it in (e/GeV)².
- `KalmanFittingFactory:diagnostic_event`: default -1 disables the per-layer trace; a nonnegative event ID enables innovation χ² and KF-state logging for that event.

The original generator flag list below is retained for reference; use the active
component prefix above for current commands.

### Truth Track Parameter Generator Flags

- `TruthTrackParameterGenerator:acts:seed:cov:loc0` - *1* - [mm^2] Variance for local position 0 (impact parameter)
- `TruthTrackParameterGenerator:acts:seed:cov:loc1` - *1* - [mm^2] Variance for local position 1 (z position at perigee)
- `TruthTrackParameterGenerator:acts:seed:cov:phi` - *0.050000000000000003* - [rad^2] Variance for phi angle
- `TruthTracksSeedsHitsFactory:acts:seed:cov:qoverp` - *-1* - Derived fractional-smear model; nonnegative values set an absolute variance [(e/GeV)^2]
- `TruthTrackParameterGenerator:acts:seed:cov:theta` - *0.01* - [rad^2] Variance for theta angle
- `TruthTrackParameterGenerator:acts:seed:cov:time` - *10000000000* - [ns^2] Variance for time
- `TruthTrackParameterGenerator:acts:seed:max_hits` - *3* - Maximum number of hits to include in a seed (typically 3-5 for initial seeding)
- `TruthTrackParameterGenerator:acts:seed:min_hits` - *3* - Minimum number of hits to create a seed
- `TruthTracksSeedsHitsFactory:acts:track_init:momentum_smear` - *0.1* - Dimensionless fractional momentum smearing
- `TruthTrackParameterGenerator:acts:use_true_hit_position` - *1* - Use true hits xyz instead of digitized one
- `TruthTrackParameterGenerator:InputTags` - *DigitizedMtpcMcTracks* - Input collection names
- `TruthTrackParameterGenerator:loglevel` - *info* - Component log level
- `TruthTrackParameterGenerator:OutputTags` - *TruthTrackSeeds,TruthTrackParameters,TrackerHits,Measurements2D* - Output collection names
- `TruthTracksSeedsHitsFactory:log_level` - *info* - log_level for TruthTracksSeedsHitsFactory: trace, debug, info, warn, error, critical, off

### Service Component Log Levels

- `JGlobalRootLock:loglevel` - *info* - Component log level
- `tdis::io::CsvWriterProcessor:loglevel` - *info* - Component log level
- `tdis::io::PodioWriteProcessor:loglevel` - *info* - Component log level
- `tdis::services::LogService:loglevel` - *info* - Component log level
- `tdis::tracking::ActsGeometryService:loglevel` - *info* - Component log level

---

## JANA Framework Flags

JANA framework provides the core event processing system. These flags control JANA's behavior.

- `jana:affinity` - *0* - Constrain worker thread CPU affinity. 0=Let the OS decide. 1=Avoid extra memory movement at the expense of using hyperthreads. 2=Avoid hyperthreads at the expense of extra memory movement (Advanced parameter)
- `jana:backoff_interval` - *10* - Max time (in seconds) JANA will wait for 'initial' events to complete before hard-exiting
- `jana:debug_plugin_loading` - *0* - Trace the plugin search path and display any loading errors
- `jana:global_loglevel` - *info* - Global log level
- `jana:inspect` - *0* - Controls whether to drop immediately into the Inspector upon Run()
- `jana:jana:wiring_file` - *(empty)* - Path to TOML file containing wiring definitions
- `jana:jana:wiring_strictness` - *1* - Allow multiple definitions inside wiring files
- `jana:locality` - *0* - Constrain memory locality. 0=No constraint. 1=Events stay on the same socket. 2=Events stay on same NUMA domain. 3=Events stay on same core. 4=Events stay on same cpu/hyperthread (Advanced parameter)
- `jana:log:show_group` - *0* - Show threadstamp in log output
- `jana:log:show_level` - *1* - Show threadstamp in log output
- `jana:log:show_threadstamp` - *0* - Show threadstamp in log output
- `jana:log:show_timestamp` - *1* - Show timestamp in log output
- `jana:loglevel` - *info* - Component log level
- `jana:max_inflight_events` - *1* - The number of events which may be in-flight at once. Should be at least `nthreads` to prevent starvation; more gives better load balancing (Advanced parameter)
- `jana:nevents` - *0* - Max number of events that sources can emit
- `jana:nskip` - *0* - Number of events that sources should skip before starting emitting
- `jana:output_processed_event_numbers` - *0* - Write the sequence of processed event numbers to file
- `jana:parameter_strictness` - *1* - 0: Ignore unused parameters, 1: Warn on unused parameters, 2: Throw on unused parameters
- `jana:parameter_verbosity` - *1* - 0: Don't show parameters table, 1: Show user-provided parameters only, 2: Show defaulted parameters, 3: Show defaulted advanced parameters
- `jana:plugin_path` - *(empty)* - Colon-separated list of paths to search for plugins
- `jana:show_ticker` - *1* - Controls whether the ticker is visible
- `jana:status_fname` - */tmp/jana_status* - Filename of named pipe for retrieving instantaneous status info
- `jana:ticker_interval` - *500* - Controls the ticker interval (in ms)
- `jana:timeout` - *8* - Max time (in seconds) JANA will wait for a thread to update its heartbeat before hard-exiting. 0 to disable timeout completely
- `jana:warmup_timeout` - *30* - Max time (in seconds) JANA will wait for 'initial' events to complete before hard-exiting
- `nthreads` - *1* - Desired number of worker threads, or 'Ncores' to use all available cores

---

## Plugin and General Flags

- `autoactivate` - *(empty)* - List of factories to activate regardless of what the event processors request. Format is typename:tag,typename:tag
- `event_source_type` - *(empty)* - Manually specifies which JEventSource should open the input file
- `plugins` - *(empty)* - Comma-separated list of plugins to load
- `plugins_to_ignore` - *(empty)* - Comma-separated list of plugins to NOT load, even if they are specified in 'plugins'
- `record_call_stack` - *0* - Records a trace of who called each factory. Reduces performance but necessary for plugins such as janadot (Advanced parameter)

---

## Notes

- Log levels can be: trace, debug, info, warn, error, critical, off
- ACTS log levels can be: VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL
- Advanced parameters should be modified with caution as they affect core framework behavior
- Flags can be set via command line using `-p<flag>=<value>` syntax or programmatically via `JApplication::SetParameterValue()`
