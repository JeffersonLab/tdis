
# TDIS

- [GitHub](https://github.com/JeffersonLab/tdis ':target=_blank')

### C++ ACTS + JANA2 based tracking for TDIS experiment

- [ACTS](https://acts.readthedocs.io/en/latest)
- [JANA2](https://jeffersonlab.github.io/JANA2)

## ACTS

ACTS is an experiment-independent toolkit for (charged) particle track reconstruction 
in high energy and nuclear physics experiments implemented in modern C++.

The ACTS project provides high-level track reconstruction modules that can be used for any tracking detector. 
The tracking detector geometry description is optimized for efficient navigation 
and fast extrapolation of tracks. In addition to the algorithmic code, this project also provides 
an event data model for the description of track parameters and measurements.

## JANA2

JANA2 is a developed at Jefferson Lab C++ framework for multi-threaded HENP event reconstruction.
It is very efficient at multi-threading with a design that makes it easy for less experienced programmers
to contribute pieces to the larger reconstruction project. The same JANA2 program can be used to easily
do partial or full reconstruction, fully maximizing the available cores for the current job.

Its design intent is to make it easy to do both: run on a laptop for local code development, 
but to also be highly efficent when deploying to large computing
sites like [NERSC](http://www.nersc.gov/ ":target=_blank").

The project is [hosted on GitHub](https://github.com/JeffersonLab/JANA2)
