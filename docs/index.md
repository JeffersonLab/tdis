
# TDIS

TDIS is the experiment at Jefferson Lab to investigate tagged deep inelastic scattering (TDIS) 
by measuring high W2, Q2 electrons scattered from hydrogen and deuterium targets in coincidence
with low momentum recoiling protons. This is a pioneering experiment that will 
probe the elusive mesonic content of the nucleon, using the tagging technique to
isolate for example scattering from the pion in proton to pion fluctuations.

The experiment will be implemented at the Thomas Jefferson National Acceleration Facility 
in Hall A utilizing Super BigBite spectrometer for electron detection, in conjunction with the thin target, solenoid and GEM-based radial time projection chamber. These combined systems, along with the
CEBAF high current CW beam, leverage the high luminosity and unique kinematics
required to access the proposed physics.

[Read more on it](https://www.jlab.org/exp_prog/proposals/14/PR12-14-010.pdf)

## tdis software

The **[tdis](https://github.com/JeffersonLab/tdis ":target=_blank")** \* software is ACTS + JANA-2 based package, developed for TDIS multi-TPC chamber tracking reconstruction. 

> \* - **tdis** software is usually written in small letters or using code quotes like `tdis` to distinguish from TDIS experiment


### ACTS

ACTS is an experiment-independent toolkit for (charged) particle track reconstruction
in high energy and nuclear physics experiments implemented in modern C++.
The ACTS project provides high-level track reconstruction modules that can be used for any tracking detector.
The tracking detector geometry description is optimized for efficient navigation
and fast extrapolation of tracks. In addition to the algorithmic code, this project also provides
an event data model for the description of track parameters and measurements.

- [ACTS Documentation](https://acts.readthedocs.io/en/latest ":target=_blank")
- [ACTS on GitHub](https://github.com/acts-project/acts/ ":target=_blank")

### JANA2

JANA2 is a developed at Jefferson Lab C++ framework for multi-threaded HENP event reconstruction.
It is efficient at multi-threading with a design that makes it easy for less experienced programmers
to contribute pieces to the larger reconstruction project. The same JANA2 program can be used 
do partial or full reconstruction, maximizing the available cores for the current job. 
Run on a laptop for local code development, but to also be highly efficient when deploying to local farms or large computing sites like [NERSC](http://www.nersc.gov/ ":target=_blank").

- [JANA2 Documentation](https://jeffersonlab.github.io/JANA2)
- [JANA2 on GitHub](https://github.com/JeffersonLab/JANA2 ":target=_blank")

[approach.md](approach.md ':include')

**Please proceed to [Architecture page](architecture.md)**
