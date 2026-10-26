#!/usr/bin/env bash
# Inside ./go_tdis, from /worksim:
#   bash tdis/python/run_tracking_performance.sh INPUT.txt -n 100
#   bash tdis/python/run_tracking_performance.sh INPUT.txt -n 100 --analyze
set -euo pipefail

usage() {
    echo "Usage: $0 INPUT.txt [-n EVENTS] [-o NEW_OUTPUT_DIR] [--analyze]"
    echo "Run inside ./go_tdis. Defaults: 100 events, new tdis/tmp/performance-* directory."
}

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
events=100
output=
analyze=false
input=
while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--events|-o|--output-dir)
            [[ $# -ge 2 ]] || { usage >&2; exit 2; }
            case "$1" in
                -n|--events) events=$2 ;;
                -o|--output-dir) output=$2 ;;
            esac
            shift 2 ;;
        --analyze) analyze=true; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
        *)
            [[ -z "$input" ]] || { usage >&2; exit 2; }
            input=$1; shift ;;
    esac
done
[[ -f "$input" ]] || { echo "Input file not found: $input" >&2; usage >&2; exit 2; }
[[ "$events" =~ ^[1-9][0-9]*$ ]] || { echo "EVENTS must be a positive integer." >&2; exit 2; }
[[ -x "$repo/build/install/bin/tdis" ]] || { echo "Build/install TDIS first." >&2; exit 2; }

if [[ -n "$output" ]]; then
    # mkdir rejects existing directories so previous runs cannot be overwritten.
    mkdir -p -- "$(dirname -- "$output")"
    mkdir -- "$output"
else
    mkdir -p -- "$repo/tmp"
    output=$(mktemp -d "$repo/tmp/performance-XXXXXX")
fi
output=$(cd -- "$output" && pwd)
echo "Output: $output"

# Keep the signed field and requested count available to the plotters.
cat > "$output/run.json" <<EOF
{"requested_events": $events, "bz_T": -4.7, "random_seed": 42,
 "momentum_smear": 0.1, "qop_variance": -1, "chi2_cut": 0,
 "hit_position": "auto", "diagnostic_event": -1}
EOF

# Reconstruct digitized events; auto uses finite truth XYZ, otherwise pad/drift positions.
# Bz = -4.7 T follows the EPC positive-charge seed convention.
(
    set -x
    "$repo/build/install/bin/tdis" \
        -pnthreads=1 \
        "-pjana:nevents=$events" \
        -pKalmanFittingFactory:bz=-4.7 \
        -pTruthTracksSeedsHitsFactory:acts:track_init:random_seed=42 \
        -pTruthTracksSeedsHitsFactory:acts:track_init:momentum_smear=0.1 \
        -pTruthTracksSeedsHitsFactory:acts:seed:cov:qoverp=-1 \
        -pKalmanFittingFactory:chi2_cut=0 \
        -pKalmanFittingFactory:diagnostic_event=-1 \
        "-ptdis:output=$output/reco" "$input"
) 2>&1 | tee "$output/reconstruction.log"
if grep -q 'appears to be unused' "$output/reconstruction.log"; then
    echo "Unused reconstruction parameters; rebuild/install the current code. See $output/reconstruction.log" >&2
    exit 1
fi

if [[ "$analyze" == true ]]; then
    python3 "$repo/python/plot_tracking_performance.py" \
        "$output/reco.tdisedm.root" -o "$output/performance" \
        --requested-events "$events" 2>&1 | tee "$output/analysis.log"
    cat "$output/performance/summary.md"
    echo "Plots: $output/performance/performance.pdf"
fi
