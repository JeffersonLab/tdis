# TDIS CSV Analysis Requirements

requirements.txt for the CSV analysis scripts

```
pandas>=1.3.0
numpy>=1.21.0
matplotlib>=3.4.0
hist>=2.5.0
```

## Installation

```bash
pip install pandas numpy matplotlib hist
```

## Usage

### Analyze track data:
```bash
python csv_analyze_in_tracks.py output.in_tracks.csv
```

### Analyze hit data:
```bash
python csv_analyze_in_hits.py output.in_hits.csv
```

### Options:
- `--output-dir`: Specify custom output directory
- `--max-rows`: Limit number of rows to read (for testing)

## Output

The scripts will create directories:
- `plots_in_tracks/` - Contains all track-related histograms and plots
- `plots_in_hits/` - Contains all hit-related histograms and plots

Each directory will contain:
- Individual histograms for each column
- Correlation plots between related variables
- Summary plots showing key distributions
- Resolution plots (for hits analysis)
