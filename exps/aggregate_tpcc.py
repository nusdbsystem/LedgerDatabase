import sys
from glob import glob

numSuccess = 0.0
sumSuccess = 0.0
numTotal = 0.0
sumTotal = 0.0
duration = 0.0
numNO = 0.0
sumNO = 0.0
numPM = 0.0
sumPM = 0.0
numOS = 0.0
sumOS = 0.0
numDL = 0.0
sumDL = 0.0
numSL = 0.0
sumSL = 0.0

path = sys.argv[1]
outpath = sys.argv[2]
for f in glob(path + "/client*log"):
  with open(f, "r") as fp:
    lines = fp.read().splitlines()
    numSuccess = numSuccess + float(lines[0])
    sumSuccess = sumSuccess + float(lines[1])
    numTotal = numTotal + float(lines[2])
    sumTotal = sumTotal + float(lines[3])
    duration = float(lines[4])
    numNO = numNO + float(lines[5])
    sumNO = sumNO + float(lines[6])
    numPM = numPM + float(lines[7])
    sumPM = sumPM + float(lines[8])
    numOS = numOS + float(lines[9])
    sumOS = sumOS + float(lines[10])
    numDL = numDL + float(lines[11])
    sumDL = sumDL + float(lines[12])
    numSL = numSL + float(lines[13])
    sumSL = sumSL + float(lines[14])

outfile = open(outpath, "w")
outfile.write(str(numSuccess/duration) + "\n")
outfile.write(str(sumSuccess/numSuccess) + "\n")
outfile.write(str(numTotal/duration) + "\n")
outfile.write(str(sumTotal/numTotal) + "\n")
outfile.write(str((numTotal - numSuccess)/numTotal) + "\n")
outfile.write(str(sumNO/numNO) + "\n")
outfile.write(str(sumPM/numPM) + "\n")
outfile.write(str(sumOS/numOS) + "\n")
outfile.write(str(sumDL/numDL) + "\n")
outfile.write(str(sumSL/numSL) + "\n")
