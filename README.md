# Ledger Database
In this repository, we emulate Amazon's Quantumn Ledger Database and Alibaba's LedgerDB.

## Dependency
* rocksdb (&geq; 5.8)
* boost (&geq; 1.67)
* protobuf (&geq; 2.6.1)
* libevent (&geq; 2.1.12)
* cryptopp (&geq; 6.1.0)
* Intel Threading Building Block (tbb_2020 version)
* openssl
* cmake (&geq; 3.12.2)
* gcc (&geq; 5.5)

## Setup
1. In `exps/env.sh`, fill in project root directory and master node ip address (where experiments will be started, usually should be current machine).
2. List server IP addresses in `exps/replicas`, client IP address in `exps/clients`, and timeservers IP address in `exps/timeservers`.
3. Build the cluster on all machines with the scripts provided

For LedgerDB
```
$ cd exps
$ ./sync.sh
$ ./build.sh ledgerdb
```
For QLDB
```
$ cd exps
$ ./sync.sh
$ ./build.sh qldb
```
Make sure the parent folder of project root directory is available on all machines.

## Run experiments
1. Configure experiment settings in `exps/run_all.sh`.

All client nodes specified in `exps/clients` will spawn client processes when running experiments.
You can specify client process started per client node (nclients) to control the total number of client processes.

You can specify nshards to control the number of servers spawned for the experiment.

2. Run experiments

```
$ cd exps
$ ./run_all.sh
```
3. Check throughput, latency and abort rate in `exps/result/tps_<wperc>_<theta>`,
where `<wperc>` and `<theta>` are write percentage and Zipf factor specified in `exps/run_all.sh`

### Example results
Experiment setting

```
Number of servers (nshards)   : 16
Client per node (nclients)    : 1 2 3 4 5 6
Number of client nodes        : 8
Write percentage (wperc)      : 50
Ziph factor (theta)           : 0
Duration in seconds (rtime)   : 300
Delay in milliseconds (delay) : 100
Transactino size (tlen)       : 10
```

Throughput of QLDB

| "#Client" | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| 16 | 5221.075 | 6864.275 | 7808.65 | 8338.675 | 8598.975 | 8841.65 |

Throughput of LedgerDB

| "#Client" | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| 16 | 11170.15 | 15836.575 | 17495.8 | 18863.625 | 20032.55 | 20051.425 |
