# PyTDIS Project Summary

# PyTDIS Quick Start Guide

## Installation

### For users:
```bash
# Install from PyPI (when published)
pip install pytdis

# Or install from source
git clone https://github.com/yourusername/pytdis
cd pytdis
pip install .
```

### For developers:
```bash
# Clone and install in development mode
git clone https://github.com/yourusername/pytdis
cd pytdis
pip install -e .[dev]
```

## Basic Usage

### Command Line
```bash
# Check version
pytdis --version

# Get help
pytdis --help

# Plot command (currently prints "Hello World!")
pytdis plot
pytdis plot data.txt -n 10
```

### Python API
```python
import pytdis

# Read 100 events as NumPy arrays
tracks, hits = pytdis.read_tdis_data_numpy('data.txt', n_events=100)
print(f"Shape: tracks={tracks.shape}, hits={hits.shape}")

# Read as Pandas DataFrame for analysis
df = pytdis.read_tdis_data_pandas('data.txt', n_events=100)
summary = pytdis.get_track_summary(df)
print(summary.head())

# Parse specific event data
from pytdis import parse_event_data
lines = ["0.5 60.0 -90.0 0.05", 
         "1000.0 1e-06 0.001 -0.05 0.08 0 90 5 0.03"]
track, hits = parse_event_data(lines, event_num=0)
```

## Running Tests
```bash
# Run all tests
python -m unittest discover tests -v

# Run specific test
python -m unittest tests.test_io.TestParseEventData
```

## Building Package
```bash
# Install build tools
pip install build twine

# Build
python -m build

# Check built packages
twine check dist/*
```

## Next Steps

1. Implement actual plotting functionality in `pytdis/cli/plot.py`
2. Add more analysis functions
3. Create visualization modules
4. Add more CLI commands
5. Write comprehensive documentation

## Publishing to PyPI

When ready to publish:
```bash
# Test on TestPyPI first
twine upload --repository testpypi dist/*

# Then upload to PyPI
twine upload dist/*
```

Remember to update:
- Version in `pytdis/version.py`
- CHANGELOG.md
- Author information in `pyproject.toml`

## Development

### Core Package Files

1. **`pyproject.toml`** - Modern Python packaging configuration
    - Defines project metadata, dependencies, and build settings
    - Ready for PyPI publication

2. **`pytdis/`** - Main package directory
    - `__init__.py` - Exports main functions from `io.py`
    - `version.py` - Version information (currently 0.1.0)
    - `__main__.py` - Allows running as `python -m pytdis`
    - `io.py` - Core functionality:
        - `parse_event_data()` - Parse individual events
        - `read_tdis_data_numpy()` - Read data as NumPy arrays
        - `read_tdis_data_pandas()` - Read data as Pandas DataFrame
        - `get_track_summary()` - Generate track statistics

3. **`pytdis/cli/`** - Command-line interface
    - `__init__.py` - Main CLI group with version flag
    - `plot.py` - Plot command (currently prints "Hello World!")

### Tests

4. **`tests/`** - Unit tests using `unittest` library
    - `test_io.py` - Comprehensive tests for all I/O functions
    - Tests include: parsing, reading, error handling, consistency checks

### Documentation

5. **`README.md`** - Main project documentation
6. **`DEVELOPMENT.md`** - Developer guide
7. **`QUICK_START.md`** - Quick start guide
8. **`CHANGELOG.md`** - Version history

### Supporting Files

9. **`LICENSE`** - MIT License
10. **`MANIFEST.in`** - Package manifest for distribution
11. **`requirements.txt`** - Direct dependency list
12. **`.gitignore`** - Git ignore patterns
13. **`setup.py`** - Backward compatibility
14. **`.github/workflows/tests.yml`** - CI/CD workflow
15. **`examples/basic_usage.py`** - Usage examples

## Key Features

- **Flexible data reading**: NumPy arrays or Pandas DataFrames
- **Event filtering**: Skip events or limit number read
- **Multi-index DataFrame**: Organized by (track_id, hit_id)
- **Track summaries**: Aggregate statistics per track
- **Extensible CLI**: Easy to add new commands
- **Comprehensive tests**: Using Python's built-in unittest
- **Ready for PyPI**: Complete packaging setup

## Next Steps

1. **Update author info** in `pyproject.toml`
2. **Implement plotting** in `pytdis/cli/plot.py`
3. **Add visualization modules** (matplotlib/plotly)
4. **Extend CLI** with more analysis commands
5. **Publish to PyPI** when ready

## How to Use

```bash
# Install for development
pip install -e .[dev]

# Run tests
python -m unittest discover tests -v

# Try the CLI
pytdis --version
pytdis plot

# Use the API
python examples/basic_usage.py
```

The package structure follows Python best practices and is ready for further development and distribution!