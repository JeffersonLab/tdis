#!/usr/bin/env python3
"""Run TDIS and its single-particle performance analysis, normally in Podman."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='Digitized TDIS input text file')
    parser.add_argument('-n', '--events', type=int, default=100)
    parser.add_argument('--bz', type=float, default=-4.7,
                        help='Signed Bz [T]; -4.7 matches EPC bending for positive-charge seeds')
    parser.add_argument('--seed', type=int, default=42, help='Positive RNG seed for paired studies')
    parser.add_argument('--momentum-smear', type=float, default=0.1,
                        help='Fractional seed momentum smearing (0 disables it)')
    parser.add_argument('--qop-variance', type=float, default=-1.,
                        help='Seed q/p variance [(e/GeV)^2]; -1 derives it from smearing')
    parser.add_argument('--diagnostic-event', type=int, default=-1,
                        help='Event ID for per-layer KF trace in reconstruction.log; -1 disables')
    parser.add_argument('--hit-position', choices=['auto', 'truth', 'digitized'], default='auto',
                        help='auto uses finite truth XYZ, otherwise pad centres and drift z')
    parser.add_argument('-o', '--output-dir', type=Path, help='New directory; existing paths are rejected')
    parser.add_argument('--image', default='eicdev/tdis-pre:latest')
    parser.add_argument('--local', action='store_true', help='Run directly, e.g. inside go_tdis')
    parser.add_argument('--executable', type=Path, default=repo / 'build/install/bin/tdis')
    args = parser.parse_args()
    if args.events <= 0:
        parser.error('--events must be positive')
    import math
    if not math.isfinite(args.bz):
        parser.error('--bz must be finite')
    if args.seed <= 0 or not math.isfinite(args.momentum_smear) or args.momentum_smear < 0:
        parser.error('--seed must be positive and --momentum-smear finite and nonnegative')
    if not math.isfinite(args.qop_variance) or (args.qop_variance < 0 and args.qop_variance != -1):
        parser.error('--qop-variance must be nonnegative or -1')
    input_file = args.input.resolve()
    executable = args.executable.resolve()
    if not input_file.is_file() or not executable.is_file():
        parser.error('Input and installed executable must exist; build/install TDIS first')
    if not args.local and not shutil.which('podman'):
        parser.error('podman is unavailable; use --local inside the TDIS image')
    if args.output_dir:
        output = args.output_dir.resolve()
        if output.exists():
            parser.error(f'Output already exists: {output}')
        output.mkdir(parents=True)
    else:
        (repo / 'tmp').mkdir(exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix='performance-', dir=repo / 'tmp'))

    image_id = None
    if args.local:
        reco_exe, reco_input, reco_output = str(executable), str(input_file), str(output)
        analysis = [sys.executable, str(repo / 'python/plot_tracking_performance.py')]
        launch = []
    else:
        # Pass argv directly; filenames and parameter values never become shell code.
        launch = ['podman', 'run', '--rm',
                  # ROOT's generated dictionary records build-time /worksim paths.
                  '-v', f'{repo.parent}:/worksim:ro',
                  '-v', f'{executable.parent.parent}:/tdis-install:ro',
                  '-v', f'{input_file.parent}:/tdis-input:ro',
                  '-v', f'{output}:/tdis-output', '-w', '/tdis-output', args.image]
        reco_exe = '/tdis-install/bin/' + executable.name
        reco_input, reco_output = '/tdis-input/' + input_file.name, '/tdis-output'
        analysis = ['python3', f'/worksim/{repo.name}/python/plot_tracking_performance.py']
        image_id = subprocess.check_output(
            ['podman', 'image', 'inspect', args.image, '--format', '{{.Id}}'], text=True).strip()
    reco_command = launch + [reco_exe, '-pnthreads=1', f'-pjana:nevents={args.events}',
                             f'-pKalmanFittingFactory:bz={args.bz}',
                             f'-pTruthTracksSeedsHitsFactory:acts:track_init:random_seed={args.seed}',
                             f'-pTruthTracksSeedsHitsFactory:acts:track_init:momentum_smear={args.momentum_smear}',
                             f'-pTruthTracksSeedsHitsFactory:acts:seed:cov:qoverp={args.qop_variance}',
                             f'-pKalmanFittingFactory:diagnostic_event={args.diagnostic_event}',
                             f'-ptdis:output={reco_output}/reco', reco_input]
    if args.hit_position != 'auto':
        reco_command.insert(len(launch) + 1,
            '-pTruthTracksSeedsHitsFactory:acts:use_true_hit_position=' +
            ('true' if args.hit_position == 'truth' else 'false'))
    analysis_command = launch + analysis + [f'{reco_output}/reco.tdisedm.root',
        '-o', f'{reco_output}/performance', '--requested-events', str(args.events)]
    git_head = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    git_status = subprocess.check_output(['git', '-C', str(repo), 'status', '--short'], text=True)
    manifest = dict(created_utc=datetime.now(timezone.utc).isoformat(), input=str(input_file),
                    input_sha256=sha256(input_file), executable=str(executable),
                    executable_sha256=sha256(executable), git_head=git_head,
                    git_status=git_status, requested_events=args.events, bz_T=args.bz,
                    random_seed=args.seed, momentum_smear=args.momentum_smear,
                    qop_variance=args.qop_variance, diagnostic_event=args.diagnostic_event,
                    hit_position=args.hit_position, image=None if args.local else args.image,
                    image_id=image_id, reconstruction_command=reco_command,
                    analysis_command=analysis_command,
                    analysis_sha256=sha256(repo / 'python/plot_tracking_performance.py'),
                    note='Fixed RNG sequence; reconstruction uses one thread for paired comparisons.')
    manifest_path = output / 'run.json'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Output: {output}', flush=True)
    for stage, command in [('reconstruction', reco_command), ('analysis', analysis_command)]:
        log = output / f'{stage}.log'
        with log.open('w') as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
        manifest[f'{stage}_exit_code'] = result.returncode
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
        if result.returncode:
            raise SystemExit(f'{stage} failed ({result.returncode}); see {log}')
        if stage == 'reconstruction' and 'appears to be unused' in log.read_text():
            raise SystemExit(f'Unused reconstruction parameters; rebuild/install the current code. See {log}')
    print((output / 'performance/summary.md').read_text())
    print(f'Plots: {output / "performance/performance.pdf"}')


if __name__ == '__main__':
    main()
