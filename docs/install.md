# TDIS Package Installation Guide

?> _RUN_ preconfigured and precompiled `tdis` software
?> - [Use containers](containers.md)
?> - [IFarm tutorial](ifarm-tutorial.md)

This page covers the installation process for the `tdis` package.

**The installation is standard for CMake packages** 

## Prerequisites

### Build Requirements
- **CMake**: Version 3.24 or higher
- **C++ Compiler**: Supporting C++20 standard
- **Git**: For fetching dependencies

### Required Dependencies
The following packages must be installed before building `tdis`. 
[eicdev/tdis-pre container](containers.md) container with preinstalled
and configured requirements can be used. At least this container was designed for it. 

1. **JANA** - Jefferson Lab's multi-threaded framework
2. **podio** - Event data model library
3. **fmt** - String formatting library
4. **Boost** - C++ libraries
5. **ROOT** - CERN Data analysis framework
6. **Acts** - A Common Tracking Software with components:
    - Core
    - PluginTGeo
    - PluginJson
    - ActsExamplesFramework library

### Automatically Fetched Dependencies
The following dependencies are automatically downloaded by CMake during the build process:
- **CLI11** - Command line parser
- **spdlog** - Fast C++ logging library

## Installation Steps


```bash

# 1. Clone the Repository
git clone https://github.com/JeffersonLab/tdis
cd tdis

# 2. Configure with CMake
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..

# For custom configurations, you can specify other flags like:
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install \
      -DCMAKE_CXX_STANDARD=23 \
      -DCMAKE_BUILD_TYPE=Release \
      ..
# 3. Build
cmake --build . -- -j8

# 4. Install
cmake --install .
```

This will install:
- `tdis` executable to `<install_prefix>/bin/`
- `podio_model_lib` library to `<install_prefix>/lib/`

## Run

To run

```bash
tdis
-pnthreads=1
-pjana:nevents=10
-ppodio:output_file=/mnt/data/test_output_v01.root
-pacts:geometry=/mnt/data/g4sbs_mtpc.root
-pacts:round_tgeo_values=0
-pacts:output_obj=/mnt/data/acts_geom.obj
-pacts:output_ply=/mnt/data/acts_geom.ply
-ptracking:hit_reco:log_level=trace
/mnt/data/g4sbsout_EPCEvents_200000.txt
```

To print specific collection before the output
```bash 
-ppodio:print_collections=TrackerHit
```

### Docker Usage
While this guide covers manual installation, TDIS is commonly run using Docker containers. When building custom Docker images, ensure all required dependencies are installed before building TDIS.

## Build Options

### Enable Tests (Optional)
If Catch2 v3 is available, you can enable tests:
```bash
cmake -DWITH_TESTS=ON ..
```

## Troubleshooting

### Installed executable cannot find the data model libraries

If `tdis` reports `libpodio_model_dict.so` or `libpodio_model_lib.so` is missing
after installing to `/usr/local`, run this inside the container's Bash shell:

```bash
export LD_LIBRARY_PATH="/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
tdis --help
```

This is a workaround for older builds whose installed runtime search path
omits the project's own libraries. The Linux CMake configuration now includes
`$ORIGIN/../lib` for the executable and `$ORIGIN` for the data model libraries,
so rebuilding and reinstalling removes the need for that workaround.
See the [CMake INSTALL_RPATH documentation](https://cmake.org/cmake/help/latest/prop_tgt/INSTALL_RPATH.html).

For a persistent installation using the host mount
`-v /work/halla/tdis/shujie:/worksim`, run inside the container:

```bash
cd /worksim/tdis
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/worksim/tdis/build/install
cmake --build build -j2
cmake --install build
export PATH="/worksim/tdis/build/install/bin:$PATH"
tdis --help
```

The build and installation remain in the mounted directory when a container
started with `--rm` exits; installations under `/usr/local` do not. In a new
container using the same mount, repeat the `export PATH` command or invoke
`/worksim/tdis/build/install/bin/tdis` directly. Run this executable inside the
dependency image, where ROOT, JANA, Acts, and podio are available.

### Acts Installation Path
The build system automatically detects Acts installation. If Acts is installed in a non-standard location, ensure that:
- `Acts_DIR` or `CMAKE_PREFIX_PATH` includes the Acts installation directory
- ActsExamplesFramework library is available in the Acts lib directory

### Missing Dependencies
If CMake reports missing packages, ensure that:
- Package `*_DIR` variables point to the correct installation paths
- `CMAKE_PREFIX_PATH` includes all dependency installation directories

Example:
```bash
cmake -DCMAKE_PREFIX_PATH="/path/to/jana;/path/to/acts;/path/to/podio" ..
```

## Verification
After installation, verify the executable:
```bash
<install_prefix>/bin/tdis --help
```
