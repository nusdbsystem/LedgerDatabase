import sys

def process(op, infile):
  lat = []
  with open(infile, "r") as fp:
    for line in fp:
      if line.startswith(op+" "):
        lat.append(float(line.split(" ")[1]))
  return lat

if __name__ == "__main__":

  path = sys.argv[1]                                                                 
  wpers = sys.argv[2].split(",")                                                     
  servers = sys.argv[3].split(",")                                                   
  clients = sys.argv[4].split(",")
  ops = sys.argv[5].split(",")

  #ops=["get", "verify", "prepare", "commit", "abort"]

  for w in wpers:
    fout = open(path + "/avg_" + w, "w")
    for s in servers:
      fout.write("\n" + s + "\n")
      fout.write("Op")
      for c in clients:
        fout.write("\t" + c)
      fout.write("\n")
      
      for op in ops:
        fout.write(op)
        for c in clients:
          n = c #str(int(c) * int(s))
          lat = process(op, path + "/" + op + "_" + w + "_" + s + "_" + n)
          fout.write("\t" + str(sum(lat)/len(lat)))
        fout.write("\n")
