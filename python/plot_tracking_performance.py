#!/usr/bin/env python3
"""Analyze TDIS schema-102 ROOT output using truth/seed/trajectory relations."""

import argparse
from array import array
import csv
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import tempfile
import textwrap
import warnings

import numpy as np
import ROOT


METRICS = {
    'dp_over_p_percent': ('(p_{fit}-p_{truth})/p_{truth} [%]', '%'),
    'dtheta_mrad': ('#theta_{fit}-#theta_{truth} [mrad]', 'mrad'),
    'dphi_mrad': ('wrapped #phi_{fit}-#phi_{truth} [mrad]', 'mrad'),
    'dca_r_mm': ('Signed DCA_{r} = perigee loc.a [mm]', 'mm'),
}


def valid(row):
    return row['perigee_success'] in (True, 'True')


def conditional_harmonics(rows):
    """First/second phi harmonics after p/theta quartile-stratum intercepts.

    HC3 covariance allows different residual variances across the sample.
    These are diagnostic regressions; fitted tracking parameters are unchanged.
    """
    rows = [r for r in rows if valid(r)]
    if len(rows) < 200:
        return dict(status='fewer than 200 valid perigees')
    p = np.array([float(r['p_truth_GeV']) for r in rows])
    theta = np.array([float(r['theta_truth_rad']) for r in rows])
    phi = np.array([float(r['phi_truth_rad']) for r in rows])
    groups = np.searchsorted(np.quantile(p, [.25, .5, .75]), p) * 4 + np.searchsorted(
        np.quantile(theta, [.25, .5, .75]), theta)
    _, group_index = np.unique(groups, return_inverse=True)
    intercepts = np.eye(group_index.max() + 1)[group_index]
    harmonics = np.column_stack([np.sin(phi), np.cos(phi), np.sin(2*phi), np.cos(2*phi)])
    design = np.column_stack([intercepts, harmonics])
    if np.linalg.matrix_rank(design) < design.shape[1]:
        return dict(status='insufficient phi/kinematic coverage')
    inverse = np.linalg.inv(design.T @ design)
    leverage = np.sum(design * (design @ inverse), axis=1)
    result = dict(status='ok', n=len(rows), strata=intercepts.shape[1], metrics={})
    for metric in METRICS:
        values = np.array([float(r[metric]) for r in rows])
        coefficients = inverse @ design.T @ values
        residual = values - design @ coefficients
        variance = (residual / np.maximum(1 - leverage, 1e-6))**2
        covariance = inverse @ ((design.T * variance) @ design) @ inverse
        beta = coefficients[-4:]
        harmonic_covariance = covariance[-4:, -4:]
        if np.linalg.matrix_rank(harmonic_covariance) < 4:
            result['metrics'][metric] = dict(coefficients=beta.tolist(),
                status='degenerate residual covariance', probability=None)
            continue
        statistic = float(beta @ np.linalg.solve(harmonic_covariance, beta))
        result['metrics'][metric] = dict(coefficients=beta.tolist(), wald_chi2=statistic,
            ndf=4, probability=float(ROOT.TMath.Prob(statistic, 4)))
    return result


def phi_report(rows, directory):
    """Show raw phi distributions; paired rotations test algorithmic symmetry."""
    if any((directory / name).exists() for name in
           ['phi_dependence.png', 'phi_dependence.pdf', 'phi_bins.csv', 'phi_summary.json']):
        raise FileExistsError('Phi diagnostic output already exists; choose a new report directory')
    ROOT.gROOT.SetBatch(True)
    rng = np.random.default_rng(42)
    conditional = conditional_harmonics(rows)
    edges = np.linspace(-math.pi, math.pi, 9)
    results, tests, objects = [], {}, []
    canvas = ROOT.TCanvas('phi_dependence', 'Phi diagnostics', 1500, 1500)
    canvas.Divide(2, 4)
    for index, (metric, (label, _)) in enumerate(METRICS.items()):
        bins = []
        for low, high in zip(edges[:-1], edges[1:]):
            values = np.array([float(r[metric]) for r in rows if valid(r) and
                low <= (float(r['phi_truth_rad']) + math.pi) % (2 * math.pi) - math.pi < high], dtype=float)
            record = dict(metric=metric, phi_low=float(low), phi_high=float(high),
                          n=int(values.size), mean=None, mean_error=None,
                          std=None, width68=None, width68_error=None)
            if values.size >= 10:
                width = float(np.diff(np.quantile(values, [.16, .84]))[0] / 2)
                resamples = rng.choice(values, size=(400, values.size), replace=True)
                widths = np.diff(np.quantile(resamples, [.16, .84], axis=1), axis=0)[0] / 2
                record.update(mean=float(values.mean()), mean_error=float(values.std(ddof=1) / math.sqrt(values.size)),
                              std=float(values.std(ddof=1)), width68=width,
                              width68_error=float(widths.std(ddof=1)))
            results.append(record)
            bins.append(record)
        tests[metric] = {}
        for panel, (value_key, error_key, title) in enumerate([
                ('mean', 'mean_error', 'Mean residual'), ('width68', 'width68_error', 'Central 68% half-width')]):
            canvas.cd(2 * index + panel + 1)
            selected = [b for b in bins if b[value_key] is not None and b[error_key] > 0]
            if not selected:
                continue
            x = [(b['phi_low'] + b['phi_high']) / 2 for b in selected]
            y = [b[value_key] for b in selected]
            errors = [b[error_key] for b in selected]
            graph = ROOT.TGraphErrors(len(x), array('d', x), array('d', y),
                                     array('d', [(b['phi_high'] - b['phi_low']) / 2 for b in selected]),
                                     array('d', errors))
            graph.SetTitle(f'{title};Truth #phi [rad];{label}')
            graph.SetMarkerStyle(20)
            graph.Draw('AP')
            weights = 1 / np.square(errors)
            constant = float(np.sum(weights * y) / weights.sum())
            chi2 = float(np.sum(weights * np.square(np.array(y) - constant)))
            ndf = len(y) - 1
            probability = float(ROOT.TMath.Prob(chi2, ndf)) if ndf > 0 else None
            tests[metric][value_key] = dict(constant=constant, chi2=chi2, ndf=ndf,
                                           probability=probability)
            line = ROOT.TLine(-math.pi, constant, math.pi, constant)
            line.SetLineStyle(2)
            line.Draw()
            text = ROOT.TLatex()
            text.SetNDC(True)
            text.SetTextSize(.035)
            if probability is not None:
                text.DrawLatex(.15, .85, f'Constant fit p = {probability:.3g}')
            if panel == 0 and conditional.get('status') == 'ok':
                controlled = conditional['metrics'][metric].get('probability')
                if controlled is not None:
                    text.DrawLatex(.15, .79, f'p/#theta-controlled harmonics p = {controlled:.3g}')
            objects.extend([graph, line, text])
    canvas.Print(str(directory / 'phi_dependence.png'))
    canvas.Print(str(directory / 'phi_dependence.pdf'))
    with (directory / 'phi_bins.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(results[0]))
        writer.writeheader()
        writer.writerows(results)
    report = dict(tests=tests, bins=results, conditional_harmonics=conditional,
                  note='Raw phi bins can mix different p/theta spectra. Width errors use 400 bootstrap resamples. '
                       'Paired rotations hold p, theta, seed and measurements fixed and directly test algorithmic phi symmetry.')
    (directory / 'phi_summary.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    return report


def wrap_phi(value):
    return math.atan2(math.sin(value), math.cos(value))


def efficiency(passed, total):
    """Wilson interval with z=1 (68.27%); nonzero uncertainty at 0%/100%."""
    if not 0 <= passed <= total:
        raise ValueError('Efficiency requires 0 <= passed <= total')
    if total == 0:
        return dict(passed=passed, total=total, value=None, low=None, high=None)
    value = passed / total
    center = (value + 0.5 / total) / (1 + 1 / total)
    half = math.sqrt(value * (1 - value) / total + 0.25 / total**2) / (1 + 1 / total)
    return dict(passed=passed, total=total, value=value,
                low=max(0., min(value, center - half)),
                high=min(1., max(value, center + half)))


def read_values(tree, expression, entry):
    with warnings.catch_warnings():
        # Empty fitted collections are expected for failed fits.
        warnings.filterwarnings('ignore', message='The selected TTree subset is empty.',
                                category=RuntimeWarning)
        count = tree.Draw(expression, '', 'goff', 1, entry)
    if count < 0:
        raise ValueError(f'Cannot read {expression}')
    if count > tree.GetEstimate():
        tree.SetEstimate(count + 1)
        count = tree.Draw(expression, '', 'goff', 1, entry)
    return [tree.GetV1()[i] for i in range(count)]


def read_tracks(path):
    # PODIO metadata dictionaries are standard image dependencies. No TDIS
    # dictionaries are loaded: event data are read as numeric split leaves.
    ROOT.gInterpreter.AddIncludePath('/app/podio/include')
    if ROOT.gSystem.Load('libpodioRootIODict') < 0:
        raise RuntimeError('PODIO ROOT metadata library missing; use the TDIS image')
    with warnings.catch_warnings():
        warnings.filterwarnings('ignore', message='no dictionary for class .* is available',
                                category=RuntimeWarning)
        source = ROOT.TFile.Open(str(path), 'READ')
    if not source or source.IsZombie():
        raise ValueError(f'Cannot open {path}')
    try:
        tree = source.Get('events')
        metadata = source.Get('podio_metadata')
        if not tree or not metadata:
            raise ValueError('Expected events and podio_metadata TTrees')
        metadata.GetEntry(0)
        collections = {str(c.name): (int(c.collectionID), int(c.schemaVersion))
                       for c in metadata.events___CollectionTypeInfo}
        for name in ['DigitizedMtpcMcTracks', 'TruthTrackSeeds', 'FittedTrajectories',
                     'FittedTracks', 'FittedTrackParameters']:
            if name not in collections or collections[name][1] != 102:
                raise ValueError(f'{name}: expected schema 102 with fitted perigee status')
        rows, seen_events = [], set()
        diagnostics = dict(exported_tracks=0, cylinder_fallbacks=0, invalid_perigees=0,
                           duplicate_fits=0, seedless_truth=0)
        for entry in range(tree.GetEntries()):
            def values(name):
                return read_values(tree, name, entry)

            def relation(name, index, target, target_size):
                indices = values(name + '.index')
                ids = values(name + '.collectionID')
                if index >= len(indices) or int(ids[index]) != collections[target][0]:
                    raise ValueError(f'Entry {entry}: invalid {name} collection relation')
                result = int(indices[index])
                if not 0 <= result < target_size:
                    raise ValueError(f'Entry {entry}: invalid {name} index {result}')
                return result

            event, run = values('EventInfo.EventNumber'), values('EventInfo.RunNumber')
            if len(event) != 1 or len(run) != 1:
                raise ValueError(f'Entry {entry}: expected one EventInfo')
            event_key = (int(run[0]), int(event[0]))
            if event_key in seen_events:
                raise ValueError(f'Duplicate run/event {event_key}; cannot safely join records')
            seen_events.add(event_key)
            momentum = values('DigitizedMtpcMcTracks.momentum')
            theta = values('DigitizedMtpcMcTracks.theta')
            phi = values('DigitizedMtpcMcTracks.phi')
            vertex_z = values('DigitizedMtpcMcTracks.vertexZ')
            event_rows = [dict(run=event_key[0], event=event_key[1], entry=entry,
                mc_index=i, p_truth_GeV=p, theta_truth_rad=theta[i], phi_truth_rad=phi[i],
                vertex_z_truth_mm=vertex_z[i], has_seed=False, fit_success=False,
                perigee_success=False, n_fits=0, track_index=-1,
                **{key: None for key in METRICS}) for i, p in enumerate(momentum)]
            nseeds = len(values('_TruthTrackSeeds_mcTrack.index'))
            seed_mc = [relation('_TruthTrackSeeds_mcTrack', i, 'DigitizedMtpcMcTracks', len(momentum))
                       for i in range(nseeds)]
            for index in seed_mc:
                event_rows[index]['has_seed'] = True
            ntrajectories = len(values('FittedTrajectories.type'))
            track_types = values('FittedTracks.type')
            parameter_types = values('FittedTrackParameters.type')
            qop = values('FittedTrackParameters.qOverP')
            fitted_theta = values('FittedTrackParameters.theta')
            fitted_phi = values('FittedTrackParameters.phi')
            loc_a = values('FittedTrackParameters.loc.a')
            perigee_surfaces = values('FittedTrackParameters.surface==0')
            for track_index, track_type in enumerate(track_types):
                diagnostics['exported_tracks'] += 1
                trajectory_index = relation('_FittedTracks_trajectory', track_index,
                                             'FittedTrajectories', ntrajectories)
                seed_index = relation('_FittedTrajectories_seed', trajectory_index,
                                      'TruthTrackSeeds', nseeds)
                row = event_rows[seed_mc[seed_index]]
                row['n_fits'] += 1
                row['fit_success'] = True
                if int(track_type) == 1:
                    diagnostics['cylinder_fallbacks'] += 1
                    continue
                if int(track_type) != 0:
                    raise ValueError(f'Unexpected fitted Track.type={track_type}')
                begin = int(values('FittedTrajectories.trackParameters_begin')[trajectory_index])
                end = int(values('FittedTrajectories.trackParameters_end')[trajectory_index])
                indices = [relation('_FittedTrajectories_trackParameters', i,
                            'FittedTrackParameters', len(parameter_types)) for i in range(begin, end)]
                perigees = [i for i in indices if int(parameter_types[i]) == 0 and perigee_surfaces[i]]
                if len(perigees) != 1:
                    raise ValueError(f'Entry {entry}: successful perigee must have one parameter record')
                i = perigees[0]
                truth_p = row['p_truth_GeV']
                finite = all(math.isfinite(x) for x in
                    [qop[i], fitted_theta[i], fitted_phi[i], loc_a[i], truth_p,
                     row['theta_truth_rad'], row['phi_truth_rad']])
                if not finite or qop[i] == 0 or truth_p <= 0:
                    diagnostics['invalid_perigees'] += 1
                    continue
                # Count each truth particle once. For duplicates retain the first
                # valid perigee in collection order, without a chi2/residual cut.
                if row['perigee_success']:
                    continue
                row.update(perigee_success=True, track_index=track_index,
                    dp_over_p_percent=100 * (abs(1 / qop[i]) - truth_p) / truth_p,
                    dtheta_mrad=1000 * (fitted_theta[i] - row['theta_truth_rad']),
                    dphi_mrad=1000 * wrap_phi(fitted_phi[i] - row['phi_truth_rad']),
                    dca_r_mm=loc_a[i])
            diagnostics['duplicate_fits'] += sum(max(0, r['n_fits'] - 1) for r in event_rows)
            diagnostics['seedless_truth'] += sum(not r['has_seed'] for r in event_rows)
            rows.extend(event_rows)
        return rows, len(seen_events), diagnostics
    finally:
        source.Close()


def distribution(values, name):
    """Full-sample widths plus an ePIC-style robust-window Gaussian core fit."""
    values = np.asarray(values, dtype=float)
    values = values[np.isfinite(values)]
    summary = dict(n=int(values.size), mean=None, std=None, width68=None,
                   gaussian_mean=None, gaussian_sigma=None, gaussian_sigma_error=None,
                   gaussian_status='fewer than 50 finite entries', core_count=0,
                   fit_low=None, fit_high=None, gaussian_chi2_ndf=None)
    if not values.size:
        return summary, None, None
    summary.update(mean=float(np.mean(values)),
                   std=float(np.std(values, ddof=1)) if values.size > 1 else None,
                   width68=float((np.quantile(values, 0.84) - np.quantile(values, 0.16)) / 2))
    center = float(np.median(values))
    scale = float(1.4826 * np.median(np.abs(values - center)))
    if scale <= 0:
        scale = float(np.std(values))
    # Match the example's robust median/MAD initialization and 3-sigma clipping.
    for _ in range(5):
        if scale <= 0:
            break
        core = values[np.abs(values - center) <= 3 * scale]
        if core.size < 50:
            break
        next_center = float(np.median(core))
        next_scale = float(1.4826 * np.median(np.abs(core - next_center)))
        if next_scale <= 0:
            break
        center, scale = next_center, next_scale
    display_scale = scale if scale > 0 else max(abs(center) * 0.1, 1e-3)
    low, high = center - 4 * display_scale, center + 4 * display_scale
    histogram = ROOT.TH1D(name, '', max(15, min(60, int(2 * math.sqrt(values.size)))), low, high)
    histogram.SetDirectory(0)
    for value in values:
        histogram.Fill(float(value))
    if scale <= 0:
        summary['gaussian_status'] = 'degenerate distribution'
        return summary, histogram, None
    fit_low, fit_high = center - 3 * scale, center + 3 * scale
    core_count = int(np.count_nonzero((values >= fit_low) & (values <= fit_high)))
    summary.update(core_count=core_count, fit_low=fit_low, fit_high=fit_high)
    if core_count < 50:
        return summary, histogram, None
    function = ROOT.TF1(name + '_gaussian', 'gaus', fit_low, fit_high)
    function.SetParameters(max(1., histogram.GetMaximum()), center, scale)
    function.SetParLimits(0, 0., 10 * values.size)
    function.SetParLimits(1, fit_low, fit_high)
    function.SetParLimits(2, histogram.GetBinWidth(1) / 10, fit_high - fit_low)
    # Poisson likelihood includes empty bins. Fit only the declared core window.
    result = histogram.Fit(function, 'QSLIR0N')
    valid = (int(result) == 0 and result.IsValid() and result.CovMatrixStatus() >= 2
             and math.isfinite(function.GetParError(2)) and function.GetParError(2) > 0
             and histogram.GetBinWidth(1) / 10 < function.GetParameter(2) < fit_high - fit_low)
    if valid:
        summary.update(gaussian_mean=float(function.GetParameter(1)),
                       gaussian_sigma=float(function.GetParameter(2)),
                       gaussian_sigma_error=float(function.GetParError(2)), gaussian_status='ok',
                       gaussian_chi2_ndf=float(function.GetChisquare() / function.GetNDF())
                       if function.GetNDF() > 0 else None)
        return summary, histogram, function
    summary['gaussian_status'] = f'fit failed/unreliable (status {int(result)})'
    return summary, histogram, None


def bin_edges(values, bins=8):
    values = np.asarray([x for x in values if math.isfinite(x)], dtype=float)
    if not values.size:
        return np.array([0., 1.])
    low, high = float(values.min()), float(values.max())
    if low == high:
        low, high = low - max(abs(low) * .05, .01), high + max(abs(high) * .05, .01)
    return np.linspace(low, high, bins + 1)


def plot_report(rows, directory, source, run_info):
    ROOT.gROOT.SetBatch(True)
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetTitleFontSize(.045)
    pdf = str(directory / 'performance.pdf')
    canvas = ROOT.TCanvas('performance', 'TDIS tracking performance', 1400, 1000)
    canvas.Print(pdf + '[')
    keep = []
    binned, metric_summaries = [], {}
    try:
        title = ROOT.TText()
        title.SetNDC(True)
        title.SetTextSize(.04)
        title.DrawText(.08, .85, 'TDIS single-particle tracking performance')
        title.SetTextSize(.025)
        description = [f'ROOT: {source.name}',
                       'Sample: ' + str(run_info.get('input', source)),
                       'UTC: ' + datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M'),
                       f'Truth tracks: {len(rows)}; exported fits: {sum(r["fit_success"] for r in rows)}; '
                       f'valid perigees: {sum(r["perigee_success"] for r in rows)}',
                       'Bz [T]: ' + str(run_info.get('bz_T', 'see reconstruction configuration')),
                       'Hit positions: ' + str(run_info.get('hit_position', 'see reconstruction configuration')),
                       'Truth-assisted fitting; field-only transport; no chi2 selection.',
                       'DCAr is signed perigee loc.a, relative to truth DCAr = 0.',
                       'Gaussian core fits require 50 entries. See summary.md for full-sample widths.']
        description = [part for line in description for part in textwrap.wrap(line, width=105)]
        for i, line in enumerate(description):
            title.DrawText(.08, .73 - .05 * i, line)
        canvas.Print(pdf)
        canvas.Clear()
        canvas.Divide(2, 2)
        for pad, (key, (label, units)) in enumerate(METRICS.items(), 1):
            canvas.cd(pad)
            summary, hist, function = distribution(
                [r[key] for r in rows if r['perigee_success']], key)
            metric_summaries[key] = summary
            if hist:
                hist.SetTitle(f';{label};Tracks')
                hist.SetLineColor(ROOT.kBlue + 1)
                hist.Draw('hist')
                keep.append(hist)
                if function:
                    function.SetLineColor(ROOT.kRed)
                    function.Draw('same')
                    keep.append(function)
            text = ROOT.TLatex()
            text.SetNDC(True)
            text.SetTextSize(.035)
            lines = [f'N = {summary["n"]}']
            if summary['n']:
                lines += [f'Mean = {summary["mean"]:.4g} {units}',
                          f'Central 68% half-width = {summary["width68"]:.4g} {units}',
                          f'Core window: {summary["core_count"]}/{summary["n"]} tracks']
            if summary['gaussian_sigma'] is not None:
                lines.append(f'Gaussian #sigma = {summary["gaussian_sigma"]:.4g} #pm {summary["gaussian_sigma_error"]:.2g} {units}')
            else:
                lines.append('Gaussian: ' + summary['gaussian_status'])
            for line_index, line in enumerate(lines):
                text.DrawLatex(.15, .88 - .05 * line_index, line)
            keep.append(text)
        canvas.Print(pdf)
        canvas.Print(str(directory / 'resolutions.png'))

        canvas.Clear()
        canvas.Divide(3, 1)
        for pad, (key, label) in enumerate([
            ('p_truth_GeV', 'Truth p [GeV]'),
            ('theta_truth_rad', 'Truth #theta [rad]'),
            ('phi_truth_rad', 'Truth #phi [rad]')], 1):
            canvas.cd(pad)
            edges = bin_edges([r[key] for r in rows])
            graphs = []
            for flag, color, title in [('fit_success', ROOT.kBlue + 1, 'Fit exported'),
                                       ('perigee_success', ROOT.kRed + 1, 'Valid perigee')]:
                graph = ROOT.TGraphAsymmErrors()
                graph.SetTitle(f';{label};Efficiency')
                graph.SetMarkerColor(color)
                graph.SetLineColor(color)
                graph.SetMarkerStyle(20 if flag == 'fit_success' else 24)
                for low, high in zip(edges[:-1], edges[1:]):
                    selected = [r for r in rows if low <= r[key] <= high
                                and (r[key] < high or high == edges[-1])]
                    estimate = efficiency(sum(r[flag] for r in selected), len(selected))
                    binned.append(dict(variable=key, bin_low=float(low), bin_high=float(high),
                                       selection=flag, **estimate))
                    if estimate['total']:
                        i = graph.GetN()
                        graph.SetPoint(i, (low + high) / 2, estimate['value'])
                        graph.SetPointError(i, (high - low) / 2, (high - low) / 2,
                                            estimate['value'] - estimate['low'],
                                            estimate['high'] - estimate['value'])
                graph.SetMinimum(0)
                graph.SetMaximum(1.08)
                if graph.GetN():
                    graph.Draw('AP' if not graphs else 'P SAME')
                    graphs.append((graph, title))
                    keep.append(graph)
            legend = ROOT.TLegend(.12, .18, .88, .3)
            for graph, title in graphs:
                legend.AddEntry(graph, title, 'pe')
            legend.Draw()
            keep.append(legend)
        canvas.Print(pdf)
        canvas.Print(str(directory / 'efficiency.png'))
    finally:
        canvas.Print(pdf + ']')
    return metric_summaries, binned


def write_csv(path, rows, fields=None):
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields or list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def analyze(source, directory, requested_events=None):
    rows, processed, diagnostics = read_tracks(source)
    provenance = source.parent / 'run.json'
    run_info = json.loads(provenance.read_text()) if provenance.is_file() else {}
    if requested_events is None:
        requested_events = run_info.get('requested_events')
    metrics, binned = plot_report(rows, directory, source, run_info)
    phi = phi_report(rows, directory)
    summary = dict(source=str(source), created_utc=datetime.now(timezone.utc).isoformat(),
                   requested_events=requested_events, processed_events=processed,
                   truth_tracks=len(rows), diagnostics=diagnostics, metrics=metrics,
                   run_info=run_info, phi_diagnostics=phi['tests'],
                   conditional_phi_harmonics=phi['conditional_harmonics'],
                   fit_efficiency=efficiency(sum(r['fit_success'] for r in rows), len(rows)),
                   perigee_efficiency=efficiency(sum(r['perigee_success'] for r in rows), len(rows)))
    (directory / 'summary.json').write_text(json.dumps(summary, indent=2, allow_nan=False) + '\n')
    write_csv(directory / 'tracks.csv', rows, fields=[
        'run', 'event', 'entry', 'mc_index', 'p_truth_GeV', 'theta_truth_rad', 'phi_truth_rad',
        'vertex_z_truth_mm', 'has_seed', 'fit_success', 'perigee_success', 'n_fits',
        'track_index', *METRICS])
    write_csv(directory / 'efficiency.csv', binned)
    lines = ['# TDIS tracking performance', '', f'Input: `{source}`',
             f'Processed events: {processed}; requested: {requested_events if requested_events is not None else "not specified"}.',
             f'Truth tracks: {len(rows)}; seedless: {diagnostics["seedless_truth"]}; cylinder fallbacks: {diagnostics["cylinder_fallbacks"]}.', '']
    for key, title in [('fit_efficiency', 'Fit efficiency'), ('perigee_efficiency', 'Perigee efficiency')]:
        estimate = summary[key]
        if estimate['value'] is None:
            lines.append(f'{title}: undefined (no truth tracks).')
        else:
            lines.append(f'{title}: {estimate["passed"]}/{estimate["total"]} = {100 * estimate["value"]:.2f}% '
                         f'(68.27% Wilson interval {100 * estimate["low"]:.2f}–{100 * estimate["high"]:.2f}%).')
    lines += ['', '| Residual | N | Bias (mean) | Sample SD | Central 68% half-width | Gaussian core σ ± error | Gaussian χ²/ndf |',
              '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    def number(value):
        return 'unavailable' if value is None else f'{value:.5g}'
    for key, info in metrics.items():
        gaussian = (f'{number(info["gaussian_sigma"])} ± {number(info["gaussian_sigma_error"])}'
                    if info['gaussian_sigma'] is not None else info['gaussian_status'])
        lines.append(f'| {key} | {info["n"]} | {number(info["mean"])} | {number(info["std"])} | {number(info["width68"])} | {gaussian} | {number(info["gaussian_chi2_ndf"])} |')
    lines += ['', 'Efficiency denominator: all truth tracks in processed ROOT events, with no hit-count or chi2 cut. '
              'Counts are unique truth particles, joined through track → trajectory → seed → MC relations.',
              'Resolution selection: finite, nonzero-q/p perigee states only. Phi residuals are wrapped to [−π, π]. '
              'DCAr is signed loc.a [mm], assuming the input truth vertex lies on the beam axis (truth DCAr = 0).',
              'Gaussian fits require at least 50 entries in the robust ±3σ window. Check shape and fit quality '
              'before interpreting their widths as a resolution. Full-sample statistics include tails; '
              'sample SD uses N−1. The central 68% half-width is (q84 − q16)/2.',
              'This is truth-assisted fitting efficiency for the processed hit-bearing sample, not detector acceptance '
              'or independent pattern-recognition efficiency. Seed-smearing configuration is in run.json; transport omits material effects.',
              'Phi-binned biases/widths and bootstrap uncertainties are in phi_bins.csv and phi_dependence.pdf. '
              'Raw phi bins mix input kinematics; matched rotations test algorithmic symmetry directly.']
    if requested_events is not None and processed < requested_events:
        lines += ['', f'Input ended early or the source stopped: {processed}/{requested_events} requested events processed. '
                  'Unprocessed events are not counted as fit failures. Check reconstruction.log.']
    if diagnostics['duplicate_fits']:
        lines += ['', f'Duplicate exported fits: {diagnostics["duplicate_fits"]}; first valid perigee per truth particle used.']
    (directory / 'summary.md').write_text('\n'.join(lines) + '\n')
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='Read-only schema-102 .tdisedm.root file')
    parser.add_argument('-o', '--output-dir', type=Path, help='New report directory')
    parser.add_argument('--requested-events', type=int)
    args = parser.parse_args()
    source = args.input.resolve()
    if not source.is_file():
        parser.error(f'Input does not exist: {source}')
    if args.output_dir:
        directory = args.output_dir.resolve()
        if directory.exists():
            parser.error(f'Output already exists: {directory}')
        directory.mkdir(parents=True)
    else:
        directory = Path(tempfile.mkdtemp(prefix='performance-', dir=source.parent))
    analyze(source, directory, args.requested_events)
    print((directory / 'summary.md').read_text())
    print(f'Report: {directory}')


if __name__ == '__main__':
    main()
