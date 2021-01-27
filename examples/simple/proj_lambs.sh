echo i$1_b$2_u$3_d$4_p$5_ppn$6 
mkdir logs/lambs_proj/lb_both/i$1_b$2_u$3_d$4_p$5_ppn$6 
./charmrun ./Gravity_proj -f ../../inputgen/lambs.00200 -i $1 -b $2 -u $3 -d $4 +p $5 +ppn $6 +balancer RefineLB +LBDebug 1 +setcpuaffinity +traceroot logs/lambs_proj/lb_both/i$1_b$2_u$3_d$4_p$5_ppn$6 +logsize 2000000 | tee logs/lambs_proj/lb_both/i$1_b$2_u$3_d$4_p$5_ppn$6/log

