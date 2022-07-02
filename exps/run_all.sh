#===== Parameters to Fill =====

# Write percentage
wperc=(50)

# Number of shards
nshards=(8)

# Number of client processes per client node
nclients=(1 2 3 4 5 6 7 8 9 10)

# Zipf factor
theta=(0)

# Experiment duration
rtime=120

# Verification delay
delay=100

# Transaction size
tlen=10

#==============================

wpers=$( IFS=$','; echo "${wperc[*]}" )
servers=$( IFS=$','; echo "${nshards[*]}" )
clients=$( IFS=$','; echo "${nclients[*]}" )
thetas=$( IFS=$','; echo "${theta[*]}" )

for i in ${wperc[@]}
do
  for j in ${nshards[@]}
  do
    ./load.sh $j
    for k in ${theta[@]}
    do
      for c in ${nclients[@]}
      do
        echo =============================================
        echo -e $i % writes, $j nodes, $k Zipf, $c clients
        echo =============================================

        sed -i -e "s/tlen=[0-9]*/tlen=${tlen}/g" run_test.sh
        sed -i -e "s/rtime=[0-9]*/rtime=${rtime}/g" run_test.sh
        sed -i -e "s/wper=[0-9]*/wper=$i/g" run_test.sh
        sed -i -e "s/nshard=[0-9]*/nshard=$j/g" run_test.sh
        sed -i -e "s/nclient=[0-9]*/nclient=$c/g" run_test.sh
        sed -i -e "s/zalpha=[0-9\.]*/zalpha=$k/g" run_test.sh
        sed -i -e "s/delay=[0-9\.]*/delay=${delay}/g" run_test.sh
        
        ./clean.sh
        ./run_test.sh
        ./clean.sh
      done
    done
  done
done

python parse_result.py result/ $wpers $servers $clients $thetas
