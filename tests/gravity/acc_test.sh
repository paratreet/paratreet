#!/bin/sh
# Script modified from https://github.com/N-BodyShop/tests/tree/master/forces/np/tstforce_ChaNGa.sh

echo "Testing forces in ParaTreeT"
echo -e "For HEXADECAPOLE, expect RMS errors of .0008 and max errors of .03\n"

hostname=`hostname`
testname="lambs.00200_subsamp_30K"
arr="../array"
app="../../examples"

echo "Running ParaTreeT..."
if [[ $hostname == *"lassen"* ]]; then
  # LLNL Lassen
  jsrun -n2 -a1 -c20 -K1 -r2 $app/Gravity -f $testname -v +ppn 20 +pemap L0-76:4,80-156:4 &> $testname.out
elif [[ $hostname == *"batch"* ]]; then
  # OLCF Summit
  jsrun -n2 -a1 -c21 -K1 -r2 $app/Gravity -f $testname -v +ppn 21 +pemap L0-164:4 &> $testname.out
else
  $app/charmrun $app/Gravity +p 4 -f $testname -v $testname +ppn 2 +setcpuaffinity &> $testname.out
fi

echo -e "\nBuilding and running array utility..."
make -C $arr > /dev/null

$arr/subarr lambs.00200_subsamp_30K.acc direct.acc > diff.acc
$arr/magvec < diff.acc > magdiff.arr
$arr/magvec < direct.acc > mag.acc
$arr/divarr magdiff.arr mag.acc > rdiff.acc

echo -e "\nRMS relative force error:"
$arr/rmsarr < rdiff.acc

echo "Maximum relative force error:"
$arr/maxarr < rdiff.acc

echo -e "\nCleaning up array utility..."
make -C $arr clean > /dev/null

