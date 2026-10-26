#!/usr/bin/env python3
"""Plot readout planes directly from raw digitized event text.

Requires NumPy and Matplotlib; raw plotting needs no reconstruction output,
CSV, ROOT, run manifest or KF log. Supports six-column hits and nine-column
hits with truth XYZ; readout plots always use pad centres and supplied zToGem.
Input lengths are metres, displayed lengths mm, and TDC/arrival times ns.
Event selection uses the original 'Event N' labels, including empty events.
Display one event by default, populated planes only, one plane per PDF page.
--event-range FIRST LAST overlays an inclusive range, with shared TDC colors.

Raw text format (whitespace separated; indices are zero based):

    Event N
    momentum_GeV theta_deg phi_deg vertexZ_m
    time_ns ADC ring pad plane zToGem_m

Nine-column hit rows insert truth_x_m truth_y_m truth_z_m after ADC. Every
pulse remains separate, including repeated pad/time samples. Sentinel ring
or pad indices +/-999 are skipped and counted in display.json; other invalid
indices or malformed selected records raise an error with the input line.

Geometry follows source/tdis/geometry/TdisGeometryHelper.hpp: 21 rings from 50 to
150 mm, 122 pads per ring and ten readout planes. Odd rings are staggered by
half a pad pitch. z = plane_z + zToGem on even planes and plane_z - zToGem
on odd planes; supplied zToGem is used without converting TDC to drift z.
Update the constants here if the detector geometry changes.

Run examples (from the workspace containing tdis/, using host Python):

    INPUT=EPC-4p7T-NoxyzHits-ChargeSharing/g4sbsout_EPCEvents_0.txt

    # One event; one populated plane per PDF page.
    python3 tdis/python/plot_kf_diagnostics.py "$INPUT" --event 0 -o tdis/tmp/raw-event-0

    # Overlay events 0 through 12 (inclusive), colored by TDC.
    python3 tdis/python/plot_kf_diagnostics.py "$INPUT" --event-range 0 12 -o tdis/tmp/raw-overlay-0-12

    # Separate event PDFs for the first 50 raw records, in file order.
    python3 tdis/python/plot_kf_diagnostics.py "$INPUT" --first-events 50 -o tdis/tmp/raw-first-50

    # Optional legacy KF audit: explicitly use reconstruction outputs.
    RUN=tdis/tmp/kf-hit-study-20261023/validated-primary-chi2-25
    python3 tdis/python/plot_kf_diagnostics.py "$RUN/reco" --processed --first-events 0 -o "$RUN/example-kf-audit"

Choose a new output directory each time. Only --processed mode requires the
runner's CSV/log outputs and reconstruction with --diagnostic-event -2; in
that mode --event selects the processed JANA counter instead of the raw label.

Outputs:
    Raw mode: event_NNNNNN.pdf (or events_FIRST_LAST.pdf for an overlay),
    matching per-plane PNGs and display.json with event IDs, units and counts.
    Empty events remain in metadata but produce no plane pages. TDC colors
    share one scale across all selected events/planes; overlapping XY pulses
    can be distinguished in the z/time projections.

    --processed: states.csv, candidates.csv, track_measurement_counts.csv,
    diagnostics.json and diagnostics.pdf/.png. The first two rows show rphi
    and z predicted residuals, pulls and smoothed residuals. rphi = R*phi;
    predicted residual = measured - predicted, S = P_local + V, and
    chi2 = residual^T * inverse(S) * residual before the measurement update.
    The count panel compares candidates supplied and measurements used for
    the same successfully fitted tracks. Each histogram has one entry per
    track; a higher used-count bin does not imply more used measurements.
    Selection biases the pulls, and smoothed residuals reuse fitted hits.
"""

import argparse
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.collections import PolyCollection
from matplotlib.colors import Normalize
import numpy as np


# Keep these readout coordinates and pad pitch aligned with TdisGeometryHelper.hpp.
PLANE_Z_MM = [-249.935, -150.065, -149.935, -50.065, -49.935,
              49.935, 50.065, 149.935, 150.065, 249.935]
PITCH = 2 * np.pi / 122


def read_csv(path):
    with path.open(newline='') as stream:
        return list(csv.DictReader(stream))


def write_csv(path, rows):
    if not rows:
        return
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def read_states(path):
    """Read the fitter's KF_STATE trace; its legacy u coordinate is rphi."""
    integer_fields = {'event', 'seed', 'state', 'ring', 'pad', 'plane', 'candidate',
                      'measurement', 'outlier', 'hole', 'meas_id', 'candidates'}
    rows = []
    with path.open() as stream:
        for line in stream:
            if 'KF_STATE ' not in line:
                continue
            fields = dict(token.split('=', 1) for token in line.split('KF_STATE ', 1)[1].split())
            fields = {key: {'true': '1', 'false': '0'}.get(value, value) for key, value in fields.items()}
            rows.append({key: int(value) if key in integer_fields else float(value)
                         for key, value in fields.items()})
    if not rows or 'predicted_u' not in rows[0]:
        raise ValueError('Need current layer traces: rebuild and run with --diagnostic-event -2')
    identities = [(s['event'], s['seed'], s['state']) for s in rows]
    if len(identities) != len(set(identities)):
        raise ValueError('Duplicate state traces; use a single reconstruction log')
    return rows


def stats(values):
    """Summarize finite values; SD uses N-1, and percentiles retain the tails."""
    array = np.asarray(values, dtype=float)
    array = array[np.isfinite(array)]
    if not len(array):
        return dict(n=0)
    return dict(n=len(array), mean=float(array.mean()),
                sd=float(array.std(ddof=1)) if len(array) > 1 else None,
                median=float(np.median(array)), p95=float(np.quantile(array, .95)),
                p99=float(np.quantile(array, .99)), maximum=float(array.max()))


def candidate_audit(hits, states):
    """Recompute every candidate's pre-update 2D chi2 at a traced cylinder.

    CSV measurement errors are diagonal; retain the transported rphi-z
    covariance in S. Compare the selected minimum and stored chi2 within
    CSV/float precision. Unselected candidates are not fitted outlier states.
    """
    groups = defaultdict(list)
    surfaces = {}
    for hit in hits:
        groups[(int(hit['evt']), int(hit['seed_id']), int(hit['meas_surface']))].append(hit)
        surfaces[(int(hit['evt']), int(hit['seed_id']), int(hit['meas_id']))] = int(hit['meas_surface'])
    audited = []
    for state in states:
        # The exported ID is an event-local EDM index, not a pad/cell ID.
        surface = surfaces[(state['event'], state['seed'], state['meas_id'])]
        matches = groups[(state['event'], state['seed'], surface)]
        chosen = next(h for h in matches if int(h['meas_id']) == state['meas_id'])
        if len(matches) != state['candidates']:
            raise ValueError('Candidate count does not reconcile with the hit CSV')
        radius = np.hypot(float(chosen['hit_x']), float(chosen['hit_y']))
        circumference = 2 * np.pi * radius
        selected_chi2 = None
        layer_rows = []
        for hit in matches:
            # Choose the nearest equivalent rphi across the cylinder's phi seam.
            residual_u = (float(hit['meas_loc0']) - state['predicted_u'] + circumference / 2) % circumference - circumference / 2
            residual_z = float(hit['meas_loc1']) - state['predicted_z']
            suu = state['pred_cov_uu'] + float(hit['meas_cov0'])
            suz = state['pred_cov_uz']
            szz = state['pred_cov_zz'] + float(hit['meas_cov1'])
            determinant = suu * szz - suz * suz
            if suu <= 0 or szz <= 0 or determinant <= 0:
                raise ValueError('Nonpositive innovation covariance')
            chi2 = (szz * residual_u**2 - 2 * suz * residual_u * residual_z +
                    suu * residual_z**2) / determinant
            selected = int(hit['meas_id']) == state['meas_id']
            if selected:
                selected_chi2 = chi2
                state['pull_u'] = residual_u / np.sqrt(suu)
                state['pull_z'] = residual_z / np.sqrt(szz)
            layer_rows.append(dict(event=state['event'], seed=state['seed'],
                meas_id=int(hit['meas_id']), plane=int(hit['mc_hit_plane']),
                ring=int(hit['mc_hit_ring']), pad=int(hit['mc_hit_pad']),
                selected=int(selected), accepted=int(selected and state['measurement']),
                outlier=int(selected and state['outlier']), residual_u_mm=residual_u,
                residual_z_mm=residual_z, innovation_chi2=chi2))
        if not np.isclose(selected_chi2, state['innovation_chi2'], rtol=.002, atol=.01):
            raise ValueError('Independent innovation chi2 disagrees with the KF trace')
        if selected_chi2 > min(row['innovation_chi2'] for row in layer_rows) + .02:
            raise ValueError('Selected candidate is not the minimum innovation chi2')
        if not np.isclose(state['innovation_chi2'], state['state_chi2'], rtol=2e-5, atol=2e-4):
            raise ValueError('Stored state chi2 disagrees with the innovation')
        audited.extend(layer_rows)
    return audited


def pad_polygons():
    """Pad outlines centred on the production pad angles, including odd-ring stagger."""
    polygons = []
    for ring in range(21):
        inner = 50 + ring * 100 / 21
        outer = inner + 100 / 21
        for pad in range(122):
            center = (pad + .5 * (ring % 2)) * PITCH
            angles = np.linspace(center - PITCH / 2, center + PITCH / 2, 5)
            polygons.append(np.concatenate([
                np.column_stack([outer * np.cos(angles), outer * np.sin(angles)]),
                np.column_stack([inner * np.cos(angles[::-1]), inner * np.sin(angles[::-1])])]))
    return polygons


def read_raw_events(path, event=None, event_range=None):
    """Yield selected raw records in file order, preserving original event IDs.

    Hit layouts: time_ns ADC [truth_x_m truth_y_m truth_z_m] ring pad plane
    zToGem_m. Truth XYZ is optional and unused for the pad readout display.
    """
    record = None
    selected = False
    seen = set()
    with path.open() as stream:
        for line_number, line in enumerate(stream, 1):
            fields = line.split()
            if not fields:
                continue
            if fields[0] == 'Event':
                if record is not None and selected:
                    if record['track'] is None:
                        raise ValueError(f'Event {record["event"]}: missing track header')
                    yield record
                if len(fields) != 2:
                    raise ValueError(f'Line {line_number}: expected Event followed by an integer ID')
                event_id = int(fields[1])
                if event_id in seen:
                    raise ValueError(f'Line {line_number}: duplicate raw event ID {event_id}')
                seen.add(event_id)
                selected = ((event is None or event_id == event) and
                            (event_range is None or event_range[0] <= event_id <= event_range[1]))
                record = dict(event=event_id, line=line_number, track=None, hits=[],
                              skipped_sentinel_hits=0, input_hit_columns=[])
                continue
            if record is None:
                raise ValueError(f'Line {line_number}: data before the first Event header')
            if not selected:
                continue
            if record['track'] is None:
                if len(fields) != 4:
                    raise ValueError(f'Line {line_number}: expected momentum theta phi vertexZ')
                header = [float(value) for value in fields]
                if not np.isfinite(header).all():
                    raise ValueError(f'Line {line_number}: non-finite track header')
                record['track'] = dict(momentum_GeV=header[0], theta_deg=header[1],
                                       phi_deg=header[2], vertex_z_mm=header[3] * 1000)
                continue
            if len(fields) not in (6, 9):
                raise ValueError(f'Line {line_number}: expected 6 or 9 hit columns, got {len(fields)}')
            offset = 2 if len(fields) == 6 else 5
            time, adc = float(fields[0]), float(fields[1])
            ring, pad, plane = [int(value) for value in fields[offset:offset + 3]]
            drift = float(fields[offset + 3]) * 1000  # input m -> display mm
            if len(fields) not in record['input_hit_columns']:
                record['input_hit_columns'].append(len(fields))
            if ring in (-999, 999) or pad in (-999, 999):
                record['skipped_sentinel_hits'] += 1
                continue
            if not (0 <= ring < 21 and 0 <= pad < 122 and 0 <= plane < 10):
                raise ValueError(f'Line {line_number}: invalid ring/pad/plane {ring}/{pad}/{plane}')
            if not np.isfinite([time, adc, drift]).all():
                raise ValueError(f'Line {line_number}: non-finite hit time, ADC or zToGem')
            radius = 50 + (ring + .5) * 100 / 21
            phi = (pad + .5 * (ring % 2)) * PITCH
            record['hits'].append(dict(evt=record['event'], meas_id=-1,
                mc_hit_ring=ring, mc_hit_pad=pad, mc_hit_plane=plane,
                hit_x=radius * np.cos(phi), hit_y=radius * np.sin(phi),
                hit_z=PLANE_Z_MM[plane] + (-drift if plane % 2 else drift),
                hit_time=time, adc=adc, z_to_gem_mm=drift, input_line=line_number))
        if record is not None and selected:
            if record['track'] is None:
                raise ValueError(f'Event {record["event"]}: missing track header')
            yield record


def raw_display(args, parser):
    """Select raw records and render them without opening any processed inputs."""
    if not args.source.is_file():
        parser.error(f'Raw input file does not exist: {args.source}; use --processed for a KF output prefix')
    if args.log or args.event_table or args.input:
        parser.error('--log, --event-table and --input apply only with --processed')
    if args.first_events == 0 and args.event is None and args.event_range is None:
        parser.error('Select at least one raw event; --first-events 0 is for --processed KF audits')
    records = []
    for record in read_raw_events(args.source, args.event, args.event_range):
        records.append(record)
        if args.event is not None:
            break
        if args.event_range is None and len(records) == args.first_events:
            break
    if not records:
        parser.error('No raw events match the requested selection')
    if args.event is None and args.event_range is None and len(records) < args.first_events:
        parser.error('Fewer raw events than requested for display')
    events = [record['event'] for record in records]
    hits = [hit for record in records for hit in record['hits']]
    args.output_dir.mkdir(parents=True)
    display = plane_displays(hits, [], events, args.output_dir, overlay=args.event_range is not None)
    summary = dict(input=str(args.source.resolve()), mode='raw',
        event_ids='Original Event N labels in the input text', displayed_events=events,
        position='Pad centre XY; z = plane_z +/- supplied zToGem (even +, odd -)',
        units=dict(position='mm', time='ns', input_zToGem='m'), hit_count=len(hits),
        events=[{key: value for key, value in record.items() if key != 'hits'} for record in records],
        display=display)
    (args.output_dir / 'display.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))
    print(f'Reports: {args.output_dir}')


def complete_display_hits(hits, input_path, tracks, events):
    """Add raw pulses absent from hit CSV, checking source-record/event identity.

    The current single-file reader assigns JANA counters by record ordinal,
    not the original 'Event N' label. Validate the truth header before joining.
    """
    expected = {int(t['event']): t for t in tracks}
    wanted = set(events)
    def signature(event, plane, ring, pad, time, drift):
        return event, plane, ring, pad, time, round(drift, 7)

    reconstructed = Counter(signature(int(h['evt']), int(h['mc_hit_plane']),
        int(h['mc_hit_ring']), int(h['mc_hit_pad']), float(h['mc_hit_time']),
        float(h['mc_hit_ztogem'])) for h in hits if int(h['evt']) in wanted)
    remaining = reconstructed.copy()
    extra = []
    event, header, seen = -1, None, set()
    with input_path.open() as stream:
        for line in stream:
            if line.startswith('Event'):
                event += 1
                header = None
                if event > max(events):
                    break
                continue
            if event not in wanted or not line.strip():
                continue
            data = [float(value) for value in line.split()]
            if header is None:
                if len(data) != 4:
                    raise ValueError('Expected a four-column single-track input header')
                header = data
                truth = expected[event]
                if not np.allclose(data, [float(truth['p_truth_GeV']),
                    np.degrees(float(truth['theta_truth_rad'])),
                    np.degrees(float(truth['phi_truth_rad'])),
                    float(truth['vertex_z_truth_mm']) / 1000], rtol=1e-6, atol=1e-6):
                    raise ValueError(f'Input header does not match processed event {event}')
                seen.add(event)
                continue
            time, adc, ring, pad, plane, drift = data[:6]
            ring, pad, plane = int(ring), int(pad), int(plane)
            if ring in (-999, 999) or pad in (-999, 999):
                continue
            if not (0 <= ring < 21 and 0 <= pad < 122 and 0 <= plane < 10):
                raise ValueError('Invalid input pad/plane index')
            drift *= 1000  # input metres -> EDM/display mm
            key = signature(event, plane, ring, pad, time, drift)
            if remaining[key]:
                remaining[key] -= 1
                continue
            radius = 50 + (ring + .5) * 100 / 21
            phi = (pad + .5 * (ring % 2)) * PITCH
            extra.append(dict(evt=event, meas_id=-1, mc_hit_plane=plane,
                mc_hit_ring=ring, mc_hit_pad=pad, hit_x=radius * np.cos(phi),
                hit_y=radius * np.sin(phi), hit_z=PLANE_Z_MM[plane] +
                (-drift if plane % 2 else drift), hit_time=time))
    if seen != wanted or any(remaining.values()):
        raise ValueError('Original input does not reconcile with displayed event/hit identities')
    return hits + extra, len(extra)


def plane_displays(hits, states, events, output, overlay=False):
    """Write one populated plane per page; an empty state list gives raw-only plots.

    All selected events/planes share one TDC normalization. KF associations
    include the event ID, because measurement indices repeat between events.
    """
    polygons = pad_polygons()
    by_event = defaultdict(list)
    for hit in hits:
        by_event[int(hit['evt'])].append(hit)
    chosen = {(s['event'], s['meas_id']): s for s in states}
    has_kf = bool(states)
    times = np.array([float(h['hit_time']) for event in events for h in by_event[event]])
    display = dict(overlay_events=overlay, kf_overlay=has_kf, tdc_range_ns=None, pages=[])
    if not len(times):
        print('Selected events have no hits; no plane pages written.', flush=True)
        return display
    if not np.isfinite(times).all():
        raise ValueError('Non-finite hit TDC/arrival time in selected events')
    low, high = float(times.min()), float(times.max())
    display['tdc_range_ns'] = [low, high]
    # Keep a finite color scale for a single pulse or identical arrival times.
    padding = max(abs(low) * .01, .5) if low == high else 0.
    normalization = Normalize(vmin=low - padding, vmax=high + padding)
    groups = [events] if overlay else [[event] for event in events]
    for group in groups:
        event_hits = [hit for event in group for hit in by_event[event]]
        if overlay:
            first, last = min(group), max(group)
            stem = f'events_{first:06d}_{last:06d}'
            title = f'Events {first}–{last} ({len(group)} events overlaid)'
        else:
            stem = f'event_{group[0]:06d}'
            title = f'Event {group[0]}'
        planes = sorted({int(h['mc_hit_plane']) for h in event_hits})
        if not planes:
            print(f'{title}: no hits; no plane pages written.', flush=True)
            continue
        with PdfPages(output / f'{stem}.pdf') as pdf:
            for plane in planes:
                plane_hits = [h for h in event_hits if int(h['mc_hit_plane']) == plane]
                fig = plt.figure(figsize=(12, 8), constrained_layout=True)
                grid = fig.add_gridspec(2, 2, width_ratios=[2, 1])
                axis = fig.add_subplot(grid[:, 0])
                axis.add_collection(PolyCollection(polygons, facecolors='white',
                    edgecolors='#d5d5d5', linewidths=.22, rasterized=True))
                occupancy = Counter((int(h['mc_hit_ring']), int(h['mc_hit_pad'])) for h in plane_hits)
                active = [polygons[ring * 122 + pad] for ring, pad in occupancy]
                if active:
                    axis.add_collection(PolyCollection(active, facecolors='#eeeeee',
                        edgecolors='#999999', linewidths=.3))
                tdc = np.array([float(h['hit_time']) for h in plane_hits])
                rings = [int(h['mc_hit_ring']) for h in plane_hits]
                points = axis.scatter([float(h['hit_x']) for h in plane_hits],
                    [float(h['hit_y']) for h in plane_hits], c=tdc, norm=normalization,
                    cmap='viridis', s=24, edgecolors='black', linewidths=.2)
                fig.colorbar(points, ax=axis, label='TDC / arrival time [ns]', fraction=.045, pad=.02)
                accepted = rejected = 0
                for hit in plane_hits:
                    state = chosen.get((int(hit['evt']), int(hit['meas_id'])))
                    if state:
                        accepted += state['measurement']
                        rejected += state['outlier']
                        axis.plot(float(hit['hit_x']), float(hit['hit_y']),
                            marker='x' if state['outlier'] else '+',
                            color='crimson' if state['outlier'] else 'darkgreen', ms=7, mew=1.2)
                counts = f'{len(plane_hits)} pulses / {len(occupancy)} pads'
                if has_kf:
                    counts += f'; {accepted} used / {rejected} outliers'
                axis.set(xlim=(-155, 155), ylim=(-155, 155), aspect='equal',
                    xlabel='x [mm]', ylabel='y [mm]',
                    title=f'Plane {plane}, z={PLANE_Z_MM[plane]:g} mm\n'
                          f'{counts}')
                z_label = 'reconstructed z [mm]' if has_kf else 'digitized z [mm]'
                for row, xkey, xlabel in [(0, 'hit_z', z_label),
                                          (1, 'hit_time', 'arrival time [ns]')]:
                    projection = fig.add_subplot(grid[row, 1])
                    projection.scatter([float(h[xkey]) for h in plane_hits], rings,
                        c=tdc, norm=normalization, cmap='viridis', s=12)
                    for hit in plane_hits:
                        state = chosen.get((int(hit['evt']), int(hit['meas_id'])))
                        if state:
                            projection.plot(float(hit[xkey]), int(hit['mc_hit_ring']),
                                'x' if state['outlier'] else '+', ms=6,
                                color='crimson' if state['outlier'] else 'darkgreen')
                    projection.set(xlabel=xlabel, ylabel='ring', ylim=(-1, 21))
                    projection.grid(alpha=.2)
                legend = 'Hit color: TDC [ns], shared scale.'
                if has_kf:
                    legend += ' Green +: accepted; red ×: rejected.'
                fig.suptitle(f'{title} — readout plane {plane}\n'
                    f'{legend}\n'
                    'Repeated pulses on a pad overlap in XY; time remains separate.', fontsize=11)
                pdf.savefig(fig, dpi=120)
                fig.savefig(output / f'{stem}_plane_{plane:02d}.png', dpi=120)
                plt.close(fig)
                page = dict(pdf=f'{stem}.pdf', plane=plane,
                    events=sorted({int(h['evt']) for h in plane_hits}), hits=len(plane_hits))
                if has_kf:
                    page.update(accepted=accepted, outliers=rejected)
                display['pages'].append(page)
        print(f'{title}: {len(planes)} populated planes → {stem}.pdf', flush=True)
    return display


def diagnostic_report(hits, states, candidates, output):
    """Plot KF residuals and counts for the successfully exported track cohort."""
    accepted = [s for s in states if s['measurement']]
    outliers = [s for s in states if s['outlier']]
    # Count each supplied measurement once per fitted seed, across all cylinders.
    # Failed/seedless tracks have no exported state trace and are not included.
    available = defaultdict(set)
    for hit in hits:
        available[(int(hit['evt']), int(hit['seed_id']))].add(int(hit['meas_id']))
    used = Counter((s['event'], s['seed']) for s in accepted)
    fitted_seeds = sorted({(s['event'], s['seed']) for s in states})
    track_counts = [dict(event=event, seed=seed,
        measurement_candidates=len(available[(event, seed)]), measurements=used[(event, seed)])
        for event, seed in fitted_seeds]
    write_csv(output / 'track_measurement_counts.csv', track_counts)
    candidate_counts = [row['measurement_candidates'] for row in track_counts]
    measurement_counts = [row['measurements'] for row in track_counts]
    pads = Counter((int(h['evt']), int(h['seed_id']), int(h['mc_hit_plane']),
                    int(h['mc_hit_ring']), int(h['mc_hit_pad'])) for h in hits)
    measurements_per_hit = Counter((int(h['evt']), int(h['hit_id'])) for h in hits)
    summary = dict(hit_rows=len(hits), traced_states=len(states),
        traced_events=len(set(s['event'] for s in states)), accepted=len(accepted),
        outliers=len(outliers), candidates_on_traced_surfaces=len(candidates),
        measurements_per_hit=dict(Counter(measurements_per_hit.values())),
        repeated_pads=sum(n > 1 for n in pads.values()),
        pulses_per_pad=stats(list(pads.values())),
        candidates_per_cylinder=stats([s['candidates'] for s in states]),
        candidates_per_track=stats(candidate_counts),
        measurements_per_track=stats(measurement_counts),
        innovation_chi2=stats([s['innovation_chi2'] for s in accepted]),
        outlier_chi2=stats([s['innovation_chi2'] for s in outliers]),
        over_25=sum(s['innovation_chi2'] > 25 for s in states),
        residuals={key: stats([s[key] for s in accepted]) for key in
            ['residual_u', 'residual_z', 'pull_u', 'pull_z',
             'smoothed_residual_u', 'smoothed_residual_z']})
    (output / 'diagnostics.json').write_text(json.dumps(summary, indent=2) + '\n')
    fig, axes = plt.subplots(3, 3, figsize=(14, 11), constrained_layout=True)
    # Rows 1/2: predicted residual, pull, smoothed residual for rphi/z.
    panels = [('residual_u', 'residual predicted rphi [mm]'),
              ('pull_u', 'rphi pull'),
              ('smoothed_residual_u', 'residual smoothed rphi [mm]'),
              ('residual_z', 'residual predicted z [mm]'),
              ('pull_z', 'z pull'),
              ('smoothed_residual_z', 'residual smoothed z [mm]')]
    for axis, (key, xlabel) in zip(axes.flat[:6], panels):
        values = np.array([s[key] for s in accepted])
        finite = values[np.isfinite(values)]
        if len(finite):
            axis.hist(finite, bins=80, histtype='step')
        axis.set(xlabel=xlabel, ylabel='accepted measurements', yscale='log')

    axis = axes[2, 0]
    bins = np.geomspace(.0001, max(100, max(s['innovation_chi2'] for s in states)), 80)
    axis.hist([s['innovation_chi2'] for s in accepted], bins=bins, histtype='step', label='accepted')
    if outliers:
        axis.hist([s['innovation_chi2'] for s in outliers], bins=bins, histtype='step', label='outliers')
    axis.set(xscale='log', yscale='log', xlabel='predicted residual χ² (2D)', ylabel='measurements')
    axis.legend()
    # Integer-centred bins, one entry per track in each independent distribution.
    # Used IDs are a subset per track; histogram bin heights need not be ordered.
    count_bins = np.arange(-.5, max(candidate_counts + measurement_counts) + 1.5)
    axes[2, 1].hist(candidate_counts, bins=count_bins, histtype='step', label='measurement candidates')
    axes[2, 1].hist(measurement_counts, bins=count_bins, histtype='step', label='measurements used')
    axes[2, 1].set(xlabel='number per track', ylabel='fitted tracks', yscale='log')
    axes[2, 1].legend()
    candidate_bins = np.geomspace(.0001,
        max(100, max(c['innovation_chi2'] for c in candidates)), 80)
    for selected, label in [(0, 'unselected'), (1, 'selected')]:
        axes[2, 2].hist([c['innovation_chi2'] for c in candidates if c['selected'] == selected],
            bins=candidate_bins, histtype='step', label=label)
    axes[2, 2].set(xscale='log', yscale='log', xlabel='candidate predicted residual χ²', ylabel='candidates')
    axes[2, 2].legend()
    fig.suptitle('KF hit diagnostics — rphi = Rφ\n'
                 'Predicted residuals/pulls are selection biased; smoothed residuals reuse fitted measurements.')
    fig.savefig(output / 'diagnostics.pdf')
    fig.savefig(output / 'diagnostics.png', dpi=140)
    plt.close(fig)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('source', type=Path, help='Raw digitized text file; with --processed, KF output prefix')
    parser.add_argument('--processed', action='store_true',
                        help='Optional KF audit mode using reconstruction CSV/log outputs')
    parser.add_argument('-o', '--output-dir', type=Path, required=True, help='New report directory')
    parser.add_argument('--log', type=Path, help='Defaults to RUN/reconstruction.log')
    parser.add_argument('--event-table', type=Path, help='Defaults to RUN/performance/tracks.csv')
    parser.add_argument('--input', type=Path, help='With --processed: original text input; defaults to run.json input')
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--event', type=int, help='Raw Event N label (JANA event ID with --processed)')
    selection.add_argument('--event-range', type=int, nargs=2, metavar=('FIRST', 'LAST'),
                           help='Overlay event IDs FIRST through LAST (inclusive) per plane')
    selection.add_argument('--first-events', type=int, default=1,
                           help='Display this many events in file order (default: 1); 0 audits only with --processed')
    args = parser.parse_args()
    if args.first_events < 0:
        parser.error('--first-events must be nonnegative')
    if args.event_range is not None and (args.event_range[0] < 0 or args.event_range[1] < args.event_range[0]):
        parser.error('--event-range requires 0 <= FIRST <= LAST')
    if args.output_dir.exists():
        parser.error('Output directory already exists; choose a new path')
    if not args.processed:
        raw_display(args, parser)
        return
    prefix = args.source
    hits = read_csv(Path(str(prefix) + '.hits.csv'))
    states = read_states(args.log or prefix.parent / 'reconstruction.log')
    candidates = candidate_audit(hits, states)
    events = []
    display_hits, extra_hits = hits, 0
    if args.event is not None or args.event_range is not None or args.first_events:
        # The truth table retains empty/seedless events and ROOT entry order.
        tracks = read_csv(args.event_table or prefix.parent / 'performance/tracks.csv')
        available = list(dict.fromkeys(int(t['event']) for t in sorted(tracks, key=lambda t: int(t['entry']))))
        if args.event is not None:
            if args.event not in available:
                parser.error(f'Event {args.event} is not in the processed event table')
            events = [args.event]
        elif args.event_range is not None:
            first, last = args.event_range
            events = [event for event in available if first <= event <= last]
            if not events:
                parser.error('No processed events in the requested range')
        else:
            events = available[:args.first_events]
            if len(events) < args.first_events:
                parser.error('Fewer processed events than requested for display')
        input_path = args.input
        if input_path is None:
            manifest = json.loads((prefix.parent / 'run.json').read_text())
            input_path = Path(manifest['input'])
        display_hits, extra_hits = complete_display_hits(hits, input_path, tracks, events)
    args.output_dir.mkdir(parents=True)
    write_csv(args.output_dir / 'states.csv', states)
    write_csv(args.output_dir / 'candidates.csv', candidates)
    summary = diagnostic_report(hits, states, candidates, args.output_dir)
    if events:
        summary['display'] = plane_displays(display_hits, states, events, args.output_dir,
                                           overlay=args.event_range is not None)
    summary['displayed_events'] = events
    summary['additional_unreconstructed_display_pulses'] = extra_hits
    (args.output_dir / 'diagnostics.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))
    print(f'Reports: {args.output_dir}')


if __name__ == '__main__':
    main()
