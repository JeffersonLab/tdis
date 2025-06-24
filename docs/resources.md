# Resources

*Updated: 06/23/2025* - check how current this info is 

The resources that might be needed to run the reconstruction

### TDIS ifarm space

```bash
# Space for TDIS
/work/halla/tdis

# Same as: 
/w/halla-scshelf2102/tdis
```

### g4sbs

Geant4 simulation

The branch is:

- [Official g4sbs is here](https://github.com/JeffersonLab/g4sbs)
- The fork is used [https://github.com/efuchey/g4sbs/tree/tdis_dev](https://github.com/efuchey/g4sbs/tree/tdis_dev)
- Instructions to run the program is in [https://hallaweb.jlab.org/wiki/index.php/Documentation_of_g4sbs#Documentation_of_g4sbs:_Overview](https://hallaweb.jlab.org/wiki/index.php/Documentation_of_g4sbs#Documentation_of_g4sbs:_Overview)

GDML files are located:


### Geometry

```
/w/halla-scshelf2102/sbs/efuchey/gdml_files
```

To convert gdml to root

https://github.com/eic/root2cad?tab=readme-ov-file#gdml-conversion

```bash
# Converting the geometry to root
root -e 'TGeoManager::Import("g4sbs_mTPC.gdml")->Export("g4sbs_mTPC.root")'

# Check overlaps command:
root -e 'TGeoManager::Import("g4sbs_mTPC.gdml")->Export("g4sbs_mTPC.root"); gGeoManager->CheckOverlaps(0.00000001); gGeoManager->PrintOverlaps();'
```


### MTPCSim

The other software exploring mTPC reconstruction and studies

[https://code.jlab.org/saw/mtpcsim](https://code.jlab.org/saw/mtpcsim)


### Input data


Digitized files live in:
```bash 
# Rachel format with XYZ information 1.5 T Geant4

/w/halla-scshelf2102/sbs/rmontgom/TDIS-EPCevents-xyz


```

Data description SEE HERE: [[event_format]]

# Magnetic field map

1.5T magnetic field parallel to the z axis
