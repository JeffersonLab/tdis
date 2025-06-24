## Implementation approach

While JANA-2 is a full-featured framework, at this moment `tdis` package does not
aim to be full reconstruction software for TDIS experiment and doesn't include any non-tracking features.
Instead, it's an ACTS tracking module that only aims to get all the data needed
for the reconstruction, do ACTS reconstruction, and save the output in a
straightforward way (CSV).

In some sense, it is the simplest possible approach to apply ACTS to do the tracking,
using JANA as glue framework, that helps to pass the data between parts, read the data and save sit.

If we look at a chain:

```
MC -> detector sim -> digitization -> reconstruction ... [tracking] ... -> analysis
```
This package represents only the tracking box.

It:
- takes the data from the MTPC chamber,
- applies ACTS tracking algorithms on it,
- saves the data so it can be picked up by the consequent framework or analysis.

This implies several implementation decisions:

1. Try to keep data as simple as possible and as minimal as possible.
2. Avoid bringing in our own data.
3. We use ACTS data model as much as possible and try not to invent our own data structures as much as possible.
4. It also means that we try not to adopt other blocks of reconstruction, into the code (e.g. digitization, events mixing, smearing, etc.)
5. We try to get data, process data, and spit the data out. Keeping the data simple.

This approach will allow estimating all the capabilities of ACTS package as well as do analysis
required for TDIS experiments. At the same time, if needed to integrate this package,
whether in other C++ framework or expand to be the main C++ framework for the experiment
or maybe include it as a Python package because JANA-2 provides Python bindings.