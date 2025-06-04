# PyTDIS

Python library for TDIS mTPC detector data analysis.

## Installation

```bash
pip install pytdis
```

## Quick Start

### Read detector data

```python
import pytdis

# Read as NumPy arrays
tracks, hits = pytdis.read_tdis_data_numpy('data.txt', n_events=100)

# Read as Pandas DataFrame
df = pytdis.read_tdis_data_pandas('data.txt', n_events=100)

# Get track summary statistics
summary = pytdis.get_track_summary(df)
```

### Command-line interface

```bash
# Show version
pytdis --version

# Plot detector data (coming soon)
pytdis plot data.txt -n 10
```

## Features

- **Fast I/O**: Efficient reading of TDIS detector data files
- **Flexible formats**: NumPy arrays for performance, Pandas DataFrames for analysis
- **Event selection**: Skip events or limit the number of events to read
- **Track analysis**: Built-in track summary statistics
- **CLI tools**: Command-line interface for quick data exploration
- **Pure Python**: Easy installation with minimal dependencies

## Data Format

PyTDIS reads TDIS mTPC detector data files with the following structure:

```
Event 0
    0.391797  56.63  -95.11  0.0532         # momentum theta phi z_vertex
    1143.44   7.11e-07  0.000319  ...      # time adc true_x true_y ... 
    ...
Event 1
    ...
```

## Documentation

Full documentation and examples: https://github.com/JeffersonLab/tdis

## License

MIT License