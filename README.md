[![Actions Status](https://github.com/JeffersonLab/tdis/workflows/Ubuntu/badge.svg)](https://github.com/JeffersonLab/tdis/actions)
[![Actions Status](https://github.com/JeffersonLab/tdis/workflows/Style/badge.svg)](https://github.com/JeffersonLab/tdis/actions)
[![Actions Status](https://github.com/JeffersonLab/tdis/workflows/Install/badge.svg)](https://github.com/JeffersonLab/tdis/actions)
[![codecov](https://codecov.io/gh/JeffersonLab/tdis/branch/main/graph/badge.svg)](https://codecov.io/gh/JeffersonLab/tdis)


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
