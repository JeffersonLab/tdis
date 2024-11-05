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


# Geometry IDs

The geometry description is gives us that each pad is identified by `plane`, `ring`, `pad`: 

- `plane` The mTPC will consist of 10 separate TPC volumes. Numbered 0 - 9 from upstream to downstream. 
- `ring` The pads are arranged into 21 concentric rings, The rings are numbered from 0 to 20 sequentially, with 0 being the innermost ring. 
- `pad` Each ring has 122 pads, 0 is at or closest to phi=0 and numbering is clockwise

This is convenient to encode in a singe uint32_t ID. We assign 8 bits each to plane and ring, and 16 bits to pad.
The encodePadID function packs these three values into a single uint32_t using bitwise shifts and OR operations.

```python
def encode_pad_id(plane, ring, pad):
    """Encode values into a single uint32_t: 8 bits for plane, 8 bits for ring, 16 bits for pad"""
    return (plane << 24) | (ring << 16) | pad

def decode_pad_id(pad_id):
    """decodes pad_id into plane, ring, pad """
    plane = (pad_id >> 24) & 0xFF  # Get the upper 8 bits
    ring = (pad_id >> 16) & 0xFF   # Get the next 8 bits
    pad = pad_id & 0xFFFF          # Get the lower 16 bits
    return plane, ring, pad

```