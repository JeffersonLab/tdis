"""
PyTDIS - Python library for TDIS mTPC detector data analysis

This library provides tools for reading, analyzing, and visualizing
data from the TDIS (Tagged Deep Inelastic Scattering) experiment's
multiple Time Projection Chamber (mTPC) detector.
"""

from pytdis.version import version, __version__, version_tuple
from pytdis.io import (
    read_tdis_data_numpy,
    read_tdis_data_pandas,
    get_track_summary,
    parse_event_data,
)

__all__ = [
    # Version info
    "version",
    "__version__",
    "version_tuple",
    # IO functions
    "read_tdis_data_numpy",
    "read_tdis_data_pandas",
    "get_track_summary",
    "parse_event_data",
]