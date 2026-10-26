#!/usr/bin/env python3
"""Display one fitted event without running the performance analysis.

Run inside the TDIS image (./go_tdis), from /worksim; requires PyROOT, PODIO
metadata support and NumPy. No TDIS data-model dictionaries are required.

Input: read-only schema-102 .tdisedm.root reconstruction output. --event is
the processed JANA event ID, not necessarily the original raw-text label.
The display currently supports one seed/track per event, including failed fits.
Sibling run.json supplies signed bz_T; use --bz TESLA if it is unavailable.
Optional sibling reconstruction.log supplies KF_POINT diagnostics for JSON.
Raw text and CSV files are not needed. See ../docs/schema.md for schema 102.

Output: a new directory containing event_NNNNNN.pdf (an overview followed by
one populated plane per page), overview.png, plane_PP.png, and display.json.
The JSON retains hit identities, TDC/status, fit parameters, sampled XYZ and
available KF states. No performance report is generated; inputs are unchanged.
The ROOT reader, helix propagation, pad layout and renderer are defined here.
For performance statistics, run plot_tracking_performance.py separately.

Plot conventions: full staggered pad outlines, TDC-colored hits, green plus
for used measurements and red crosses for outliers, drawn on top. The solid
black curve propagates the fitted reference in uniform Bz; its inward extension
ends at the fitted closest approach to the beam axis, without a constraint
through (0,0,0). Plane curves project the candidate-z range plus a 2 mm display
margin. KF states remain in JSON without smoothed-state markers on the plots.
Lengths are mm, momenta GeV, angles rad and arrival times ns.

Example (from /worksim; choose a new output directory):

    RUN=tdis/tmp/track-event-display-20261026/states-run
    python3 tdis/python/plot_event_display.py "$RUN/reco.tdisedm.root" \
        --event 7 -o tdis/tmp/event-7-display
"""

import argparse
from array import array
import json
import math
from pathlib import Path
import warnings

import numpy as np
import ROOT


# Readout geometry, in mm; keep aligned with TdisGeometryHelper.hpp.
PLANE_Z_MM = [-249.935, -150.065, -149.935, -50.065, -49.935,
              49.935, 50.065, 149.935, 150.065, 249.935]


def read_event(path, event_number, bz_override=None):
    """Read one event through checked PODIO relations, without TDIS dictionaries.

    Unlike TTree.Draw, typed integer leaves preserve uint64 geometry IDs.
    The fit and its candidate hits always come from the same ROOT event.
    """
    ROOT.gInterpreter.AddIncludePath('/app/podio/include')
    if ROOT.gSystem.Load('libpodioRootIODict') < 0:
        raise RuntimeError('Use the TDIS image: PODIO metadata library is missing')
    with warnings.catch_warnings():
        warnings.filterwarnings('ignore', message='no dictionary for class .* is available',
                                category=RuntimeWarning)
        source = ROOT.TFile.Open(str(path), 'READ')
    if not source or source.IsZombie():
        raise ValueError(f'Cannot open {path}')
    try:
        tree, metadata = source.Get('events'), source.Get('podio_metadata')
        if not tree or not metadata:
            raise ValueError('Expected events and podio_metadata TTrees')
        metadata.GetEntry(0)
        collections = {str(c.name): (int(c.collectionID), int(c.schemaVersion))
                       for c in metadata.events___CollectionTypeInfo}
        required = ['EventInfo', 'DigitizedMtpcMcHits', 'TruthTrackSeeds', 'TrackerHits',
                    'Measurements2D', 'FittedTracks', 'FittedTrajectories', 'FittedTrackParameters']
        if any(name not in collections or collections[name][1] != 102 for name in required):
            raise ValueError('Event display requires schema-102 hit and fit collections')

        def values(name):
            leaf = tree.GetLeaf(name)
            if not leaf:
                raise ValueError(f'Missing ROOT leaf: {name}')
            if leaf.GetTypeName() in ('Float_t', 'Double_t'):
                return [float(leaf.GetValue(i)) for i in range(leaf.GetNdata())]
            integers = [int(leaf.GetValueLong64(i)) for i in range(leaf.GetNdata())]
            if leaf.GetTypeName() in ('ULong_t', 'ULong64_t'):
                integers = [value % (1 << 64) for value in integers]
            return integers

        matches = []
        for entry in range(tree.GetEntries()):
            tree.GetEntry(entry)
            if values('EventInfo.EventNumber') == [event_number]:
                matches.append(entry)
        if len(matches) != 1:
            raise ValueError(f'JANA event {event_number}: expected one ROOT entry, found {len(matches)}')
        tree.GetEntry(matches[0])
        run = values('EventInfo.RunNumber')
        if len(run) != 1:
            raise ValueError('Expected one run number')

        def related(collection, relation, owner, target, many=False):
            """Resolve one-to-one or begin/end-indexed one-to-many relations."""
            if many:
                begin = values(f'{collection}.{relation}_begin')[owner]
                end = values(f'{collection}.{relation}_end')[owner]
            else:
                begin, end = owner, owner + 1
            indices = values(f'_{collection}_{relation}.index')
            ids = values(f'_{collection}_{relation}.collectionID')
            count = tree.GetLeaf(target + '_').GetNdata()
            if not 0 <= begin <= end <= len(indices):
                raise ValueError(f'Invalid {collection}.{relation} range')
            if any(ids[i] != collections[target][0] or not 0 <= indices[i] < count
                   for i in range(begin, end)):
                raise ValueError(f'Invalid {collection}.{relation} target')
            return indices[begin:end]

        track_types = values('FittedTracks.type')
        seed_count = tree.GetLeaf('TruthTrackSeeds_').GetNdata()
        if len(track_types) > 1 or seed_count > 1:
            raise ValueError('Event display currently supports one seed/track per event')
        candidate_ids = related('TruthTrackSeeds', 'measurements', 0, 'Measurements2D', True) if seed_count else []
        used, outliers, fit = [], [], None
        if track_types:
            trajectory = related('FittedTracks', 'trajectory', 0, 'FittedTrajectories')[0]
            seed = related('FittedTrajectories', 'seed', trajectory, 'TruthTrackSeeds')[0]
            if seed != 0 or track_types[0] not in (0, 1):
                raise ValueError('Unexpected fitted seed or reference type')
            used = related('FittedTracks', 'measurements', 0, 'Measurements2D', True)
            outliers = related('FittedTrajectories', 'outliers_deprecated', trajectory, 'Measurements2D', True)
            parameters = related('FittedTrajectories', 'trackParameters', trajectory, 'FittedTrackParameters', True)
            refs = [dict(type=values('FittedTrackParameters.type')[i],
                         surface=values('FittedTrackParameters.surface')[i],
                         loc0=values('FittedTrackParameters.loc.a')[i],
                         loc1=values('FittedTrackParameters.loc.b')[i],
                         phi=values('FittedTrackParameters.phi')[i],
                         theta=values('FittedTrackParameters.theta')[i],
                         qop=values('FittedTrackParameters.qOverP')[i]) for i in parameters]
            if (len(used), len(outliers)) != (values('FittedTrajectories.nMeasurements')[trajectory],
                                             values('FittedTrajectories.nOutliers')[trajectory]):
                raise ValueError('Measurement relation counts disagree with trajectory counts')
            fit = dict(type=track_types[0], position_mm=[values('FittedTracks.position.' + a)[0] for a in 'xyz'],
                       momentum_GeV=[values('FittedTracks.momentum.' + a)[0] for a in 'xyz'],
                       charge=values('FittedTracks.charge')[0], chi2=values('FittedTracks.chi2')[0],
                       references=refs)
        for name, ids in [('candidates', candidate_ids), ('used', used), ('outliers', outliers)]:
            if len(ids) != len(set(ids)):
                raise ValueError(f'Duplicate {name} measurement identities')
        if set(used) & set(outliers) or not (set(used) | set(outliers)) <= set(candidate_ids):
            raise ValueError('Used/outlier identities do not reconcile with candidates')

        hits = []
        surfaces = values('Measurements2D.surface')
        # Read all raw pulses only when no seed exists; they are not candidates.
        records = [(None, None, i) for i in range(tree.GetLeaf('DigitizedMtpcMcHits_').GetNdata())]
        if seed_count:
            records = []
            for measurement in candidate_ids:
                hit_ids = related('Measurements2D', 'hits', measurement, 'TrackerHits', True)
                if len(hit_ids) != 1:
                    raise ValueError('Current display expects one TrackerHit per measurement')
                raw = related('TrackerHits', 'rawHit', hit_ids[0], 'DigitizedMtpcMcHits')[0]
                records.append((measurement, hit_ids[0], raw))
        for measurement, hit, raw in records:
            plane, ring, pad = [values('DigitizedMtpcMcHits.' + key)[raw] for key in ('plane', 'ring', 'pad')]
            if not (0 <= plane < 10 and 0 <= ring < 21 and 0 <= pad < 122):
                raise ValueError('Invalid raw-hit plane/ring/pad')
            raw_xyz = [values('DigitizedMtpcMcHits.padCenter' + a)[raw] for a in 'XY']
            raw_xyz.append(PLANE_Z_MM[plane] + (1 if plane % 2 == 0 else -1) *
                           values('DigitizedMtpcMcHits.zToGem')[raw])
            xyz = [values('TrackerHits.position.' + a)[hit] for a in 'xyz'] if hit is not None else raw_xyz
            status = ('used' if measurement in used else 'outlier' if measurement in outliers else
                      'candidate' if measurement is not None else 'unassigned')
            hits.append(dict(measurement=measurement, hit=hit, raw_hit=raw, plane=plane, ring=ring, pad=pad,
                             surface=surfaces[measurement] if measurement is not None else None,
                             xyz_mm=xyz, pad_xyz_mm=raw_xyz, status=status,
                             tdc_ns=values('DigitizedMtpcMcHits.time')[raw]))
        if any(not np.isfinite(h['xyz_mm'] + h['pad_xyz_mm'] + [h['tdc_ns']]).all() for h in hits):
            raise ValueError('Non-finite selected hit position or TDC')
        result = dict(display_schema_version=1, source=str(path), entry=matches[0], run=run[0], event=event_number,
                      event_id_convention='processed JANA counter', seed=0 if seed_count else None,
                      fit=fit, hits=hits, collection_ids={k: v[0] for k, v in collections.items()},
                      counts=dict(candidates=len(candidate_ids), used=len(used), outliers=len(outliers),
                                  unselected=len(candidate_ids) - len(used) - len(outliers)))
    finally:
        source.Close()

    manifest = path.parent / 'run.json'
    run_info = json.loads(manifest.read_text()) if manifest.is_file() else {}
    bz = bz_override if bz_override is not None else run_info.get('bz_T')
    if fit and (bz is None or not math.isfinite(bz)):
        raise ValueError('Fitted display needs signed Bz: provide matching run.json or --bz TESLA')
    result.update(bz_T=bz, field_source='--bz override' if bz_override is not None else
                  str(manifest) if bz is not None else None, kf_points=[])
    log = path.parent / 'reconstruction.log'
    if log.is_file() and seed_count:
        for line in log.open():
            if 'KF_POINT ' not in line:
                continue
            fields = dict(item.split('=', 1) for item in line.split('KF_POINT ', 1)[1].split())
            if (int(fields['event']), int(fields['run']), int(fields['seed'])) != (event_number, run[0], 0):
                continue
            integers = {'version', 'event', 'run', 'seed', 'state', 'surface', 'meas_id',
                        'measurement', 'outlier', 'hole'}
            point = {k: v if k == 'stage' else int(v) if k in integers else float(v) for k, v in fields.items()}
            if point['version'] != 1 or point['stage'] not in ('predicted', 'filtered', 'smoothed'):
                raise ValueError('Unsupported KF_POINT record')
            if point['meas_id'] >= 0:
                matching = [h for h in hits if h['measurement'] == point['meas_id']]
                if len(matching) != 1 or matching[0]['surface'] != point['surface']:
                    raise ValueError('KF_POINT identity does not match the ROOT candidate')
                status = 'outlier' if point['outlier'] else 'used' if point['measurement'] else None
                if status and matching[0]['status'] != status:
                    raise ValueError('KF_POINT status does not match the ROOT measurement relation')
            if not all(math.isfinite(v) for v in point.values() if isinstance(v, float)):
                raise ValueError('Non-finite KF_POINT record')
            result['kf_points'].append(point)
        keys = [(p['state'], p['stage']) for p in result['kf_points']]
        if len(keys) != len(set(keys)):
            raise ValueError('Duplicate KF_POINT records: use this reconstruction run\'s log')
    if result['kf_points']:
        if not fit:
            raise ValueError('KF_POINT records exist for an event with no fitted ROOT track')
        # Reject a log from another fit of these same hits, even if IDs coincide.
        for ref in fit['references']:
            points = [p for p in result['kf_points'] if p['stage'] == 'smoothed' and p['surface'] == ref['surface']]
            for point in points:
                if (abs(point['loc0'] - ref['loc0']) > .001 or abs(point['loc1'] - ref['loc1']) > .001 or
                    abs(math.remainder(point['phi'] - ref['phi'], 2 * math.pi)) > 1e-5 or
                    abs(point['theta'] - ref['theta']) > 1e-5 or
                    not math.isclose(point['qop'], ref['qop'], rel_tol=1e-5)):
                    raise ValueError('KF_POINT reference disagrees with the ROOT fit; check reconstruction.log')
        result['kf_points'].sort(key=lambda p: (p['path_mm'], p['state'], p['stage']))
        result['kf_point_source'] = str(log)
    return result


def helix_points(position, momentum, charge, bz, path_mm):
    """Uniform-field trajectory versus signed 3D path length [mm].

    Momentum is GeV, Bz tesla and charge in e. np.sinc gives a stable straight
    limit at zero curvature; using path length also handles pz=0 and pT=0.
    """
    position, momentum, path_mm = np.asarray(position), np.asarray(momentum), np.asarray(path_mm)
    p = np.linalg.norm(momentum)
    if not np.isfinite(np.r_[position, momentum, charge, bz]).all() or p <= 0:
        raise ValueError('Need finite fitted parameters and positive momentum')
    phi = math.atan2(momentum[1], momentum[0])
    bend = -0.299792458 * charge * bz / (1000 * p) * path_mm
    transverse = np.hypot(*momentum[:2]) / p * path_mm * np.sinc(bend / (2 * np.pi))
    return position + np.column_stack([transverse * np.cos(phi + bend / 2),
                                      transverse * np.sin(phi + bend / 2), path_mm * momentum[2] / p])


def draw_pad_pattern():
    """Draw the hit-free pad layout in the current ROOT XY view, in mm.

    Call after drawing the axes and retain the returned graphs until saving.
    Graphs are ordered by ring * 122 + pad, matching pad_polygons() in the raw
    plotter: 21 rings, 122 pads each, and an odd-ring half-pitch stagger.
    """
    outlines = []
    pitch = 2 * np.pi / 122
    for ring in range(21):
        inner, outer = 50 + ring * 100 / 21, 50 + (ring + 1) * 100 / 21
        for pad in range(122):
            center = (pad + .5 * (ring % 2)) * pitch
            angles = np.linspace(center - pitch / 2, center + pitch / 2, 5)
            corners = np.concatenate([
                np.column_stack([outer * np.cos(angles), outer * np.sin(angles)]),
                np.column_stack([inner * np.cos(angles[::-1]), inner * np.sin(angles[::-1])])])
            corners = np.vstack([corners, corners[0]])
            outline = ROOT.TGraph(len(corners), array('d', corners[:, 0]), array('d', corners[:, 1]))
            outline.SetLineColor(ROOT.TColor.GetColor('#d5d5d5'))
            outline.SetLineWidth(1)
            outline.Draw('L SAME')
            outlines.append(outline)
    return outlines


def event_display(event, directory):
    """Write an overview and one populated-plane page; keep ROOT objects alive."""
    directory.mkdir()
    ROOT.gROOT.SetBatch(True)
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPalette(ROOT.kViridis)
    hits, fit = event['hits'], event['fit']
    times = [h['tdc_ns'] for h in hits]
    low, high = (min(times), max(times)) if times else (0., 1.)
    if low == high:
        low, high = low - .5, high + .5
    event.update(tdc_limits_ns=[low, high], plane_z_mm=PLANE_Z_MM, plane_z_margin_mm=2.,
                 trajectory_method='uniform-field propagated fitted reference', sampled_path_mm=[], sampled_xyz_mm=[],
                 units=dict(position='mm', momentum='GeV', field='T', time='ns', angles='rad', qop='e/GeV'))
    xyz = np.empty((0, 3))
    if fit:
        p = np.linalg.norm(fit['momentum_GeV'])
        omega = abs(.299792458 * fit['charge'] * event['bz_T'] / (1000 * p)) if p > 0 else 0
        # Stop on the first outward branch: no artificial full-circle returns.
        limit = min(10000., np.pi / omega) if omega > 0 else 10000.
        path = np.linspace(0, limit, max(2, int(limit / .25) + 1))
        xyz = helix_points(fit['position_mm'], fit['momentum_GeV'], fit['charge'], event['bz_T'], path)
        radius = np.hypot(xyz[:, 0], xyz[:, 1])
        outer = min(150., max([np.hypot(*h['xyz_mm'][:2]) for h in hits] or [150.]) + 100 / 42)
        stops = np.flatnonzero((radius > outer) | (np.abs(xyz[:, 2]) > 275) |
                               np.r_[False, np.diff(radius) < -1e-5])
        end = max(1, int(stops[0])) if len(stops) else len(path)
        path, xyz = path[:end], xyz[:end]
        first = min([np.hypot(*h['xyz_mm'][:2]) for h in hits if h['status'] == 'used'] or [50.])
        event.update(sampled_path_mm=path.tolist(), sampled_xyz_mm=xyz.tolist(),
                     inward_before_radius_mm=float(first))

    pdf = directory / f'event_{event["event"]:06d}.pdf'
    canvas = ROOT.TCanvas('track_event', 'Fitted event', 1600, 1000)
    canvas.SetCanvasSize(1600, 1000)
    pages = [None] + sorted({h['plane'] for h in hits})
    canvas.Print(str(pdf) + '[')
    for plane in pages:
        canvas.Clear()
        objects = []
        selected = hits if plane is None else [h for h in hits if h['plane'] == plane]

        def graph(points, color, marker=0, width=2):
            points = np.asarray(points, dtype=float)
            if not points.size:
                return
            obj = ROOT.TGraph(len(points), array('d', points[:, 0]), array('d', points[:, 1]))
            obj.SetLineColor(color)
            obj.SetLineWidth(width)
            obj.SetMarkerColor(color)
            obj.SetMarkerStyle(marker)
            obj.SetMarkerSize(1.1 if marker != 20 else .7)
            obj.Draw('P SAME' if marker else 'L SAME')
            objects.append(obj)

        for projection in ('xy', 'rz'):
            pad = ROOT.TPad('view_' + projection, '', 0 if projection == 'xy' else .60,
                            .12, .60 if projection == 'xy' else 1., .92)
            pad.SetLeftMargin(.12)
            pad.SetRightMargin(.20 if projection == 'xy' else .08)
            pad.SetBottomMargin(.12)
            pad.SetTopMargin(.064)
            pad.Draw()
            pad.cd()
            # These pad dimensions/margins give equal mm scales on the XY axes.
            if projection == 'xy':
                frame = pad.DrawFrame(-170, -170, 170, 170, ';x [mm];y [mm]')
            else:
                z_values = [h['xyz_mm'][2] for h in hits] + ([float(x) for x in xyz[:, 2]] if len(xyz) else [])
                z_min, z_max = min([-275.] + z_values), max([275.] + z_values)
                frame = pad.DrawFrame(z_min - 15, -5, z_max + 15, 165, ';z [mm];r [mm]')
            objects += [pad, frame]
            if projection == 'xy':
                outlines = draw_pad_pattern()
                objects.extend(outlines)
                for ring, cell in sorted({(h['ring'], h['pad']) for h in selected}):
                    outlines[ring * 122 + cell].SetLineColor(ROOT.kGray + 1)
            else:
                for i, z in enumerate(PLANE_Z_MM):
                    graph([[z, 50], [z, 150]], ROOT.kGray + 1 if i == plane else ROOT.kGray, width=1)
                graph([[z_min, 50], [z_max, 50]], ROOT.kGray, width=1)

            if len(xyz):
                coords = xyz[:, :2] if projection == 'xy' else np.column_stack([xyz[:, 2], np.hypot(xyz[:, 0], xyz[:, 1])])
                graph(coords, ROOT.kGray + 1, width=1)
                region = np.ones(len(xyz), dtype=bool)
                if plane is not None:
                    z = [h['xyz_mm'][2] for h in selected]
                    region = (xyz[:, 2] >= min(z) - 2) & (xyz[:, 2] <= max(z) + 2)
                # Split consecutive samples so clipping never bridges a gap.
                indices = np.flatnonzero(region)
                for segment in np.split(indices, np.flatnonzero(np.diff(indices) > 1) + 1):
                    graph(coords[segment], ROOT.kBlack, width=1)
                if fit['type'] == 0:
                    ref = fit['position_mm']
                    graph([ref[:2] if projection == 'xy' else [ref[2], np.hypot(*ref[:2])]], ROOT.kBlue + 1, 24)
            graph([[0, 0]], ROOT.kBlack, 5)
            for hit in selected:
                color = ROOT.TColor.GetColorPalette(min(ROOT.TColor.GetNumberOfColors() - 1,
                    int((hit['tdc_ns'] - low) / (high - low) * (ROOT.TColor.GetNumberOfColors() - 1))))
                pos = hit['xyz_mm']
                point = [pos[:2] if projection == 'xy' else [pos[2], np.hypot(*pos[:2])]]
                graph(point, color, 20)
            # Draw status overlays after every TDC point, with red crosses last.
            for status, color, marker in [('used', ROOT.kGreen + 2, 2), ('outlier', ROOT.kRed + 1, 5)]:
                positions = [h['xyz_mm'] for h in selected if h['status'] == status]
                points = [pos[:2] if projection == 'xy' else [pos[2], np.hypot(*pos[:2])] for pos in positions]
                graph(points, color, marker)
            if projection == 'xy':
                # A small explicit color bar avoids assigning TDC to an XY axis.
                for i in range(64):
                    box = ROOT.TBox(158, -140 + i * 280 / 64, 164, -140 + (i + 1) * 280 / 64)
                    box.SetFillColor(ROOT.TColor.GetColorPalette(int(i / 63 * (ROOT.TColor.GetNumberOfColors() - 1))))
                    box.SetLineWidth(0)
                    box.Draw()
                    objects.append(box)
                axis = ROOT.TGaxis(164, -140, 164, 140, low, high, 505, '+L')
                axis.SetTitle('Input TDC [ns]')
                axis.SetLabelSize(.025)
                axis.SetTitleSize(.03)
                axis.Draw()
                objects.append(axis)
                if plane is None and fit and fit['type'] == 0:
                    inset = ROOT.TPad('beamline_zoom', '', .16, .66, .44, .94)
                    inset.SetFillColor(ROOT.kWhite)
                    inset.SetLeftMargin(.20)
                    inset.SetBottomMargin(.20)
                    inset.Draw()
                    inset.cd()
                    span = max(5., 1.5 * np.hypot(*fit['position_mm'][:2]))
                    zoom_frame = inset.DrawFrame(-span, -span, span, span, 'Beamline zoom;x [mm];y [mm]')
                    near = np.max(np.abs(xyz[:, :2]), axis=1) <= span
                    graph(xyz[near, :2], ROOT.kBlack)
                    graph([[0, 0]], ROOT.kBlack, 5)
                    graph([fit['position_mm'][:2]], ROOT.kBlue + 1, 24)
                    objects += [inset, zoom_frame]
            canvas.cd()

        text = ROOT.TLatex()
        text.SetNDC()
        text.SetTextSize(.026)
        page_name = 'Overview' if plane is None else f'Plane {plane}: projected drift-region segment'
        text.DrawLatex(.025, .962, f'Run {event["run"]}, JANA event {event["event"]}, seed {event["seed"]}: {page_name}')
        text.SetTextSize(.019)
        counts = event['counts']
        text.DrawLatex(.025, .087, f'Candidates {counts["candidates"]} (used {counts["used"]}, outliers {counts["outliers"]}, unselected {counts["unselected"]}); page hits {len(selected)}')
        text.DrawLatex(.025, .059, 'TDC-colored hits; green + used; red #times outlier (on top); solid black curve propagated fit; blue circle perigee; black #times origin')
        if fit:
            p = np.linalg.norm(fit['momentum_GeV'])
            theta = math.acos(np.clip(fit['momentum_GeV'][2] / p, -1, 1))
            phi = math.atan2(fit['momentum_GeV'][1], fit['momentum_GeV'][0])
            text.DrawLatex(.025, .935, f'Track 0: #theta={theta:.4g} rad, #phi={phi:.4g} rad; uniform-field propagated fitted reference')
            ref = next((r for r in fit['references'] if r['type'] == 0), None)
            status = f'd0={ref["loc0"]:.3f} mm, z0={ref["loc1"]:.3f} mm' if ref else 'cylinder fallback; no saved perigee'
            text.DrawLatex(.025, .031, f'p={p:.4g} GeV, Bz={event["bz_T"]:g} T, total #chi^{{2}}={fit["chi2"]:.4g}; {status}; inward transport is field-only')
        else:
            text.DrawLatex(.025, .031, 'No fitted track; unassigned raw hits shown if no seed exists')
        canvas.Update()
        canvas.Print(str(pdf))
        canvas.Print(str(directory / ('overview.png' if plane is None else f'plane_{plane:02d}.png')))
    canvas.Print(str(pdf) + ']')
    event['pages'] = ['overview'] + [f'plane_{p:02d}' for p in pages[1:]]
    (directory / 'display.json').write_text(json.dumps(event, indent=2, allow_nan=False) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('input', type=Path, help='Read-only schema-102 .tdisedm.root file')
    parser.add_argument('--event', type=int, required=True, help='Processed JANA event ID')
    parser.add_argument('-o', '--output-dir', type=Path, required=True, help='New display directory')
    parser.add_argument('--bz', type=float, help='Signed Bz [T] override; otherwise use sibling run.json')
    args = parser.parse_args()
    source, output = args.input.resolve(), args.output_dir.resolve()
    if not source.is_file():
        parser.error(f'Input does not exist: {source}')
    if output.exists():
        parser.error(f'Output already exists: {output}')
    if args.bz is not None and not math.isfinite(args.bz):
        parser.error('--bz must be finite')
    # Validate the selection and field before creating the output directories.
    try:
        event = read_event(source, args.event, args.bz)
    except (ValueError, RuntimeError) as error:
        parser.error(str(error))
    output.parent.mkdir(parents=True, exist_ok=True)
    event_display(event, output)
    print(f'Event display: {output / f"event_{args.event:06d}.pdf"}')


if __name__ == '__main__':
    main()
