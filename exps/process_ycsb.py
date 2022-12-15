import sys

start, end = -1.0, -1.0

duration = float(sys.argv[2])
warmup = duration/3.0

tLatency = []
sLatency = []
fLatency = []

rLatency = []
wLatency = []
hLatency = []
vLatency = []
nkey = 0
nkeys = []

tExtra = 0.0
sExtra = 0.0
fExtra = 0.0

xLatency = []

for line in open(sys.argv[1]):
  if line.startswith('#') or line.strip() == "":
    continue
  if line.startswith("verifynkeys"):
    nkey = float(line[12:-1])
    continue

  line = line.strip().split()
  if not line[0].isdigit() or len(line) < 4:
    continue

  if start == -1:
    start = float(line[2]) + warmup
    end = start + warmup

  fts = float(line[2])
  
  if fts < start:
    continue

  if fts > end:
    break

  latency = int(line[3])
  status = int(line[4])
  op = int(line[5])
  ttype = -1
  extra = 0

  if status == 1 and ttype == 2:
    xLatency.append(latency)

  if op == 0:
    rLatency.append(latency)
  elif op == 1:
    wLatency.append(latency)
  elif op == 2:
    hLatency.append(latency)
  elif int(line[0]) > 0:
    nkeys.append(nkey)
    vLatency.append(latency)

  tLatency.append(latency) 
  tExtra += extra

  if status == 1:
    sLatency.append(latency)
    sExtra += extra
  else:
    fLatency.append(latency)
    fExtra += extra

if len(tLatency) == 0:
  print "Zero completed transactions.."
  sys.exit()

tLatency.sort()
sLatency.sort()
fLatency.sort()

outfile = open(sys.argv[3], "w")
outfile.write(str(len(sLatency)) + "\n")                                        
outfile.write(str(sum(sLatency)) + "\n")                                        
outfile.write(str(len(tLatency)) + "\n")                                        
outfile.write(str(sum(tLatency)) + "\n")
outfile.write(str(end - start) + "\n")
outfile.write(str(len(rLatency)) + "\n")
outfile.write(str(sum(rLatency)) + "\n")
outfile.write(str(len(wLatency)) + "\n")
outfile.write(str(sum(wLatency)) + "\n")
outfile.write(str(len(hLatency)) + "\n")
outfile.write(str(sum(hLatency)) + "\n")
outfile.write(str(len(vLatency)) + "\n")
outfile.write(str(sum(vLatency)) + "\n")
outfile.write(str(sum(nkeys)) + "\n")
# print "Transactions(All/Success): ", len(tLatency), len(sLatency)
# print "Abort Rate: ", (float)(len(tLatency)-len(sLatency))/len(tLatency)
# print "Throughput (All/Success): ", len(tLatency)/(end-start), len(sLatency)/(end-start)
# print "Average Latency (all): ", sum(tLatency)/float(len(tLatency))
# print "Median  Latency (all): ", tLatency[len(tLatency)/2]
# print "99%tile Latency (all): ", tLatency[(len(tLatency) * 99)/100]
# print "Average Latency (success): ", sum(sLatency)/float(len(sLatency))
# print "Median  Latency (success): ", sLatency[len(sLatency)/2]
# print "99%tile Latency (success): ", sLatency[(len(sLatency) * 99)/100]
# print "Extra (all): ", tExtra
# print "Extra (success): ", sExtra
# if len(xLatency) > 0:
#   print "X Transaction Latency: ", sum(xLatency)/float(len(xLatency))
# if len(fLatency) > 0:
#   print "Average Latency (failure): ", sum(fLatency)/float(len(tLatency)-len(sLatency))
#   print "Extra (failure): ", fExtra
