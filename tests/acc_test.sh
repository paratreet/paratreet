#!/bin/sh
# Script modified from https://github.com/N-BodyShop/tests/tree/master/forces/np/tstforce_ChaNGa.sh

echo "Testing forces in ParaTreeT"
echo -e "For HEXADECAPOLE, expect RMS errors of .0008 and max errors of .03\n"

hostname=`hostname`
testname="lambs.00200_subsamp_30K"

echo "Running ParaTreeT..."
if [[ $hostname == *"lassen"* ]]; then
  # LLNL Lassen
  jsrun -n2 -a1 -c20 -K1 -r2 ../examples/Gravity -f $testname -v +ppn 20 +pemap L0-76:4,80-156:4 &> $testname.out
elif [[ $hostname == *"batch"* ]]; then
  # OLCF Summit
  jsrun -n2 -a1 -c21 -K1 -r2 ../examples/Gravity -f $testname -v +ppn 21 +pemap L0-164:4 &> $testname.out
else
  ../examples/charmrun ../examples/Gravity +p 4 -f $testname -v +ppn 2 +setcpuaffinity &> $testname.out
fi

echo -e "\nBuilding and running array utility..."
cd array
make > /dev/null
cd ..

./array/subarr lambs.00200_subsamp_30K.acc direct.acc > diff.acc
./array/magvec < diff.acc > magdiff.arr
./array/magvec < direct.acc > mag.acc
./array/divarr magdiff.arr mag.acc > rdiff.acc

echo -e "\nRMS relative force error:"
./array/rmsarr < rdiff.acc

echo "Maximum relative force error:"
./array/maxarr < rdiff.acc

echo -e "\nCleaning up array utility..."
cd array
make clean > /dev/null
