# Implementation details

## Core classes

For ACTS we crete geometry in
- source/tdis/tracking/BuildMtpcDetector.hpp
- source/tdis/tracking/BuildMtpcDetectorCG.cpp

Each surface is represented by MtpcDetectorElement

- source/tdis/tracking/MtpcDetectorElement.cpp
- source/tdis/tracking/MtpcDetectorElement.hpp

The global object in JANA2 framework that has access to all of it is 

- source/tdis/tracking/ActsGeometryService.cc
- source/tdis/tracking/ActsGeometryService.h

What ACTSGeometryService do exactly:

- All initialization happens in Init method which open files and create ACTS geoemtry
- Tries to read and hold our TGeo geometry
- Tries to read material map
- Analyses TGeo geometries (while it is probably is not in use in the moment)
- Creates cylindrical detector layers (ACTS surfaces)
- Saves 3d OBJ files

Current Kalman Filter logic is located at: 

- source/tdis/tracking/KalmanFittingFactory.cpp
- source/tdis/tracking/KalmanFittingFactory.h

```c++
auto result = (*m_fitter)(sourceLinks, startParams, general_fitter_options, calibrator, tracks);
```

- sourceLinks - Basicallyh hit IDs like descibed below
- startParams - Starting track parameters (reference point, kinematic assumptions)
- general_fitter_options - Acts required objects like geometry, magnetic field, etc.)
- calibrator - Not used now
- tracks - That is where we output ACTS results

The JANA2 algorithm / Factory has two functions: 

- Configure - called once per run
- Execute - called every event
   - Create output containers
   - Copy hits to acts inputs (translate 3d hits to surfaces, make source links)
   - Create fit track assumptions (based on true data)
   - Do fit
   - Do debug output after fitting


The fit is done by m_fitter which is of class ActsExamples::ConfiguredFitter

- source/tdis/tracking/ConfiguredFitter.hpp
- source/tdis/tracking/ConfiguredKalmanFitter.cpp
- source/tdis/tracking/ConfiguredKalmanFitter.h

The idea to have separate fitter is: 
- Save state to some class 
- Allows to switch fitter easily 
- Allows to select passthrough

## Geometry IDs

ACTS needs 2 conversion from class to "number" or ID

- Surface <=> number
- Hit <=> number

**For Surface-number we just use rin_id (see below)**

**For Hit-number:** 


ACTS uses concept of "SourceLinks" that links Geometry elements with hits, calibration, results and other info.


We use `IndexSourceLinks` were each sensitive geometry element is identified by an integer. 
So we need to provide ids to sensitive elements 

The geometry description is gives us that each pad is identified by `plane`, `ring`, `pad`:

- `plane` The mTPC will consist of 10 separate TPC volumes. Numbered 0 - 9 from upstream to downstream.
- `ring` The pads are arranged into 21 concentric rings, The rings are numbered from 0 to 20 sequentially, with 0 being the innermost ring.
- `pad` Each ring has 122 pads, 0 is at or closest to phi=0 and numbering is clockwise

This is convenient to encode in a singe uint32_t ID.

```
1 000 000 * plane  +  1 000 * ring  +  pad
```