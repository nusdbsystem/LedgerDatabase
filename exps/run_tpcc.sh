#!/bin/bash

#============= Parameters to fill ============
nshard=16          # number of shards
nclient=20         # number of clients / machine
rtime=120          # duration to run
delay=100          # verification delay

wper=50             # writes percentage
zalpha=0           # zipf alpha
#============= Start Experiment =============
. env.sh

replicas=`cat replicas`
clients=`cat clients`
client="tpccClient"
store="strongstore"
mode="occ"
txnrate=120

# Print out configuration being used.
echo "Configuration:"
echo "Shards: $nshard"
echo "Clients per host: $nclient"
echo "Duration: $rtime s"
echo "Delay: $delay ms"

# Start all replicas and timestamp servers
echo "Starting TimeStampServer replicas.."
$expdir/start_replica.sh tss $expdir/shard.tss.config \
  "$bindir/timeserver" $logdir

for ((i=0; i<$nshard; i++))
do
  echo "Starting shard$i replicas.."
  $expdir/start_replica.sh shard$i $expdir/shard$i.config \
    "$bindir/$store -m $mode -e 0 -s 0 -w tpcc -f $initfile -N $nshard -n $i" $logdir
done


# Wait a bit for all replicas to start up
sleep 10

# Run the clients
echo "Running the client(s)"
count=0
for host in ${clients[@]}
do
  ssh $host "source ~/.profile; mkdir -p $logdir; $expdir/start_client.sh \"$bindir/$client \
  -c $expdir/shard -N $nshard -d $rtime -m $mode -e $err -s $skew -x $txnrate\" \
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
  ssh $host "source ~/.profile; cat $logdir/client.*.log | sort -g -k 3 > $logdir/client.log; \
             rm -f $logdir/client.*.log; mkdir -p $expdir/result; \
             python $expdir/process_tpcc.py $logdir/client.log $rtime $expdir/result/${wper}_${nshard}_${nclient}_${zalpha};
             rsync $expdir/result/${wper}_${nshard}_${nclient}_${zalpha} $master:$expdir/result/client.$host.log;"
done

echo "Processing logs"
ssh $master "source ~/.profile; python $expdir/aggregate_tpcc.py $expdir/result $expdir/result/${wper}_${nshard}_${nclient}_${zalpha}; \
             rm -f $expdir/result/client.*.log;"

