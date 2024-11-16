["Use Docker" documentation to setup environment and run](use_docker.md)

To run

```bash
tdis
-pnthreads=1
-pjana:nevents=10
-ppodio:output_file=/mnt/data/test_output_v01.root
-ppodio:print_collections=McHits
-pacts:geometry=/mnt/data/
/mnt/data/g4sbsout_EPCEvents_200000.txt
```

##

## Geometry IDs

The geometry description is gives us that each pad is identified by `plane`, `ring`, `pad`: 

- `plane` The mTPC will consist of 10 separate TPC volumes. Numbered 0 - 9 from upstream to downstream. 
- `ring` The pads are arranged into 21 concentric rings, The rings are numbered from 0 to 20 sequentially, with 0 being the innermost ring. 
- `pad` Each ring has 122 pads, 0 is at or closest to phi=0 and numbering is clockwise

This is convenient to encode in a singe uint32_t ID. 

`1 000 000 * plane  +  1 000 * ring  +  pad` 