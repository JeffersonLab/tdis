# Implementation details


## Geometry IDs

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