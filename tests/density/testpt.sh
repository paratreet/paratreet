#!/bin/bash
# Run density/SPH tests on paratreet
app=../../examples
arr=../array
make -C $arr > /dev/null
$app/SPH -i 1 -f adiabtophat_glass_28721.bin -v test

echo Density test: expect smaller than 1e-5
$arr/subarr test.den changa_test.000000.gasden > den.diff
$arr/absarr  < den.diff | $arr/maxarr
$arr/rmsarr < den.diff

echo Pressure test: expect smaller than 1e-6
$arr/subarr test.pres changa_test.000000.pres > pres.diff
$arr/absarr  < pres.diff | $arr/maxarr
$arr/rmsarr < pres.diff
echo acceleration test: expect smaller than 1e-6
$arr/subarr test.acc changa_test.000000.acc2 > acc.diff
$arr/absarr  < acc.diff | $arr/maxarr
$arr/rmsarr < acc.diff

make clean -C $arr > /dev/null
