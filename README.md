# Ledger Database
This repository includes the implementations of QLDB and LedgerDB, which are used in the paper "[GlassDB: Practical Verifiable Ledger Database Through Transparency](http://arxiv.org/abs/2207.00944)". Extended version of paper can be found [Here](doc/)

## Code Structure
- `/distributed` - A common distributed layer using two-PC for distribute transaction
- `/ledger` - Ledger database implementations
  - `/common` - common types including hashes and chunks and utility functions. 
  - `/ledgerdb` - LedgerDB implementation. The system consists of a transaction log, a **bAMT**, **clue index**, and a **ccMPT**. We implement **bAMT** by forcing the update API to take as input a batch of transaction journals. The nodes in bAMT are immutable, that is, each modification results in a new node. We implement the **clue index** by building a skip-list for each clue, and keep the mapping of each clue to its skip list's head in memory. To enable queries on keys, we create one clue for each key. As the result, a transaction may update multiple skip lists. The **ccMPT** is constructed to protect the integrity of the clue index, in which the key is the clue, and the value is the number of leaf nodes in the corresponding skip-list. When committing a new transaction, the system creates a transaction journal containing the type and parameters of the transaction, updates the clue index with it, and returns the status to the client. A background thread periodically updates the bAMT with the committed journals, and updates the ccMPT. The root hash of ccMPT and bAMT are stored in a hash chain. We do not implement the timeserver authority (TSA), which is used for security in LedgerDB, as its complexity adds extra overheads to the system.
  - `/qldb` - QLDB implementation. The system consists of a **ledger storage** and an **index storage**. The former maintains the transaction log and Merkle tree built on top of it. The latter maintains a B<sup>+</sup>-tree and data materialized from the ledger. When committing a new transaction, the system appends a new log entry containing the type, parameters and other metadata of the transaction, and updates the Merkle tree.  After that, the transaction is considered committed and the status is returned to the client.  A background thread executes the transaction and updates the B<sup>+</sup>-tree with the transaction data and metadata. We do not implement the SQL layer, which is complex and only negatively impacts the overall performance of OLTP workloads.

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
1. In `exps/env.sh`, fill in project root directory, master node ip address (where experiments will be started, usually should be current machine), and init data file path if any.
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
1. Configure experiment settings in `exps/run_exp.sh`.

All client nodes specified in `exps/clients` will spawn client processes when running experiments.
You can specify client process started per client node (nclients) to control the total number of client processes.

You can specify nshards to control the number of servers spawned for the experiment.

2. Run experiments

```
$ cd exps
$ ./run_exp.sh
```
3. Check throughput, latency and abort rate in `exps/result/tps_<wperc>_<theta>`,
where `<wperc>` and `<theta>` are write percentage and Zipf factor specified in `exps/run_exp.sh`

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
