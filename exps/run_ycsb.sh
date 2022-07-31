#!/bin/bash

#============= Parameters to fill ============
nshard=16           # number of shards
nclient=20         # number of clients / machine
rtime=120          # duration to run
delay=100          # verification delay

tlen=1            # transaction length
wper=50            # writes percentage
rper=50            # reads percentage
zalpha=0           # zipf alpha

#============= Start Experiment =============
. env.sh

replicas=`cat replicas`
clients=`cat clients`
client="verifyClient"
store="strongstore"
mode="occ"
txnrate=120

# Print out configuration being used.
echo "Configuration:"
echo "Shards: $nshard"
echo "Clients per host: $nclient"
echo "Transaction Length: $tlen"
echo "Write Percentage: $wper"
echo "Read Percentage: $rper"
echo "Get history Percentage: $((100-wper-rper))"
echo "Zipf alpha: $zalpha"

# Start all replicas and timestamp servers
echo "Starting TimeStampServer replicas.."
$expdir/start_replica.sh tss $expdir/shard.tss.config \
  "$bindir/timeserver" $logdir

for ((i=0; i<$nshard; i++))
do
  echo "Starting shard$i replicas.."
  $expdir/start_replica.sh shard$i $expdir/shard$i.config \
    "$bindir/$store -m $mode -e 0 -s 0 -N $nshard -n $i -w ycsb -k 100000" $logdir
done

# Wait a bit for all replicas to start up
sleep 10

# Run the clients
echo "Running the client(s)"
count=0
for host in ${clients[@]}
do
  ssh $host "source ~/.profile; source ~/.bashrc; mkdir -p $logdir; $expdir/start_client.sh \"$bindir/$client \
  -c $expdir/shard -N $nshard \
  -d $rtime -l $tlen -w $wper -g $rper -m $mode -e 0 -s 0 -z $zalpha -t $delay -x $txnrate\" \
  $count $nclient $logdir"

  let count=$count+$nclient
done

# Wait for all clients to exit
echo "Waiting for client(s) to exit"
for host in ${clients[@]}
do
  ssh $host "source ~/.profile; $expdir/wait_client.sh $client"
done


# Kill all replicas
echo "Cleaning up"
$expdir/stop_replica.sh $expdir/shard.tss.config > /dev/null 2>&1
for ((i=0; i<$nshard; i++))
do
  $expdir/stop_replica.sh $expdir/shard$i.config > /dev/null 2>&1
done

# Measure Throughput, Latency, Abort rate
mkdir -p $expdir/result
for host in ${clients[@]}
do
  echo $host
  ssh $host "cat $logdir/client.*.log | sort -g -k 3 > $logdir/client.log; \
             rm -f $logdir/client.*.log; mkdir -p $expdir/result; \
             source ~/.profile; python $expdir/process_ycsb.py $logdir/client.log $rtime $expdir/result/${wper}_${nshard}_${nclient}_${zalpha};
             rsync $expdir/result/${wper}_${nshard}_${nclient}_${zalpha} ${master}:$expdir/result/client.$host.log;"
done

echo "Processing logs"
ssh ${master} "source ~/.profile; python $expdir/aggregate_ycsb.py $expdir/result $expdir/result/${wper}_${nshard}_${nclient}_${zalpha}; \
               rm -f $expdir/result/client.*.log;"

