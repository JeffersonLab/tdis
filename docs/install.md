# TDIS Package Installation Guide

["Containers" documentation to setup environment and run](containers.md)

This page covers the installation process for the `tdis` package.
The installation is standard for CMake packages. 

## Prerequisites

### Build Requirements
- **CMake**: Version 3.24 or higher
- **C++ Compiler**: Supporting C++20 standard
- **Git**: For fetching dependencies

### Required Dependencies
The following packages must be installed before building `tdis`. 
We can use [eicdev/tdis-pre container](containers.md) with preinstalled
requirements. At least this container was designed for it. 

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
The following dependencies are automatically downloaded during the build process:
- **CLI11** - Command line parser
- **spdlog** - Fast C++ logging library

## Installation Steps

### 1. Clone the Repository
```bash
git clone https://github.com/JeffersonLab/tdis
cd tdis
```

### 2. Configure with CMake
```bash
# You must create a build directory.
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
```

For custom configurations, you can specify other flags like:
```bash
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install \
      -DCMAKE_CXX_STANDARD=23 \
      -DCMAKE_BUILD_TYPE=Release \
      ..
```

### 4. Build
```bash
make -j$(nproc)
```
Or specify the number of parallel jobs:
```bash
make -j8
```

### 5. Install
```bash
make install
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