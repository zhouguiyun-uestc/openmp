OpenMP-based implementation of the parallel algorithm

**Manuscript Title**: Parallel identification and filling of depressions in raster digital elevation models

This repository contains the source codes of the OpenMP-based implementation of the parallel algorithm presented in the manuscript above. These codes were used in performing the tests described in the manuscript.


The codes support floating-point GeoTIFF file format through the GDAL library. Please include GDAL library into your compilation. 

Example usages:

openmpfill zhou-onepass dem.tif dem_filled.tif //use the sequential Zhou variant to fill dem

openmpfill pl dem.tif dem_filled_pl.tif  4 //use our proposed parallel algorithm (4 threads) to fill dem

openmpfill diff dem.tif dem_filled_pl.tif //compare two dems to find out whether they are identical
