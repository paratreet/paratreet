#!/bin/bash
# Run density/SPH tests on paratreet
./SPH -i 1 -f adiabtophat_glass_28721.bin -v test

echo Density test: expect smaller than 1e-5
../array/subarr test.den changa_test.000000.gasden > den.diff
../array/absarr  < den.diff | ../array/maxarr
../array/rmsarr < den.diff

echo Pressure test: expect smaller than 1e-6
../array/subarr test.pres changa_test.000000.pres > pres.diff
../array/absarr  < pres.diff | ../array/maxarr
../array/rmsarr < pres.diff
