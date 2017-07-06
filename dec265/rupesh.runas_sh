
#!/bin/bash

echo "enter num"
read num
echo "Pthreads"
#export OMP_NUM_THREADS=$num
echo "1088 2x2"
./dec265 -q -t$num 1088p25_pedestrian_tiles2x2_filteroff.bin 
echo "1088 8x8"
./dec265 -q -t$num 1088p25_pedestrian_tiles8x8_filtersoff.bin
echo "CIF 2x2" 
./dec265 -q -t$num cif_tiles2x2.bin 
echo "CIF 8x8" 
./dec265 -q -t$num cif_tiles4x4.bin 

echo "OMP"
export OMP_NUM_THREADS=$num
echo "1088 2x2"
./dec265 -q -t0 1088p25_pedestrian_tiles2x2_filteroff.bin 
echo "1088 8x8"
./dec265 -q -t0 1088p25_pedestrian_tiles8x8_filtersoff.bin
echo "CIF 2x2" 
./dec265 -q -t0 cif_tiles2x2.bin 
echo "CIF 8x8" 
./dec265 -q -t0 cif_tiles4x4.bin 
