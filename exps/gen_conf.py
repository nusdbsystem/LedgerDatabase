import sys

n_shards = int(sys.argv[1])

replication = str(0)
if (len(sys.argv) == 3 and sys.argv[2] == "-r"):
    replication = str(1)

count = int(0)
portno = int(29)
tss_port = int(25)

infile = open("replicas", "r")
ips = infile.read().splitlines()[:n_shards]
infile.close()

for i in range(n_shards):
    outfile = open("shard" + str(i) + ".config", "w")
    outfile.write("f " + replication + "\n")
    outfile.write("replica " + ips[i] + ":517" + str(portno) + "\n")
    if replication == "1":
        outfile.write("replica " + ips[(i+1)%n_shards] + ":517" + str(portno) + "\n")
        outfile.write("replica " + ips[(i+2)%n_shards] + ":517" + str(portno) + "\n")
    outfile.close()
    portno = portno + 1

tsserverfile = open("timeservers", "r")
tsserver = tsserverfile.readline()[:-1]
tsserverfile.close()
tss = open("shard.tss.config", "w")
tss.write("f " + replication + "\n")
tss.write("replica " + tsserver + ":517" + str(tss_port) + "\n")

