import sys

def process_client(logfile, plat, clat, psize, csize):
  pl = []
  cl = []
  preq = []
  pres = []
  creq = []
  cres = []

  with open(logfile, "r") as fp:
    for line in fp:
      if line.startswith("plat"):
        pl.append(float(line[5:-1]))
      elif line.startswith("clat"):
        cl.append(float(line[5:-1]))
      elif line.startswith("preqsize"):
        preq.append(int(line[9:-1]))
      elif line.startswith("pressize"):
        pres.append(int(line[9:-1]))
      elif line.startswith("creqsize"):
        creq.append(int(line[9:-1]))
      elif line.startswith("cressize"):
        cres.append(int(line[9:-1]))

  plat.write("\t" + str(sum(pl) / len(pl)))
  clat.write("\t" + str(sum(cl) / len(cl)))
  psize.write("\t" + str(sum(preq) / len(preq) + sum(pres) / len(pres)))
  csize.write("\t" + str(sum(creq) / len(creq) + sum(cres) / len(cres)))

def  process_server(logfile, pexec, cexec):
  p = []
  c = []

  with open(logfile, "r") as fp:
    for line in fp:
      if line.startswith("prepare"):
        p.append(float(line[8:-1]))
      elif line.startswith("commit"):
        c.append(float(line[7:-1]))

  pexec.write("\t" + str(sum(p) / len(p)))
  cexec.write("\t" + str(sum(c) / len(c)))

if __name__ == "__main__":

  path = sys.argv[1]
  wpers = sys.argv[2].split(",")
  servers = sys.argv[3].split(",")
  clients = sys.argv[4].split(",")

  for wper in wpers:
    plat = open(path + "/plat_" + wper, "w")
    clat = open(path + "/clat_" + wper, "w")
    pexec = open(path + "/pexec_" + wper, "w")
    cexec = open(path + "/cexec_" + wper, "w")
    psize = open(path + "/psize_" + wper, "w")
    csize = open(path + "/csize_" + wper, "w")

    plat.write("\"#Servers\"")
    clat.write("\"#Servers\"")
    pexec.write("\"#Servers\"")
    cexec.write("\"#Servers\"")
    psize.write("\"#Servers\"")
    csize.write("\"#Servers\"")
    for c in clients:
      plat.write("\t\"" + c + "\"")
      clat.write("\t\"" + c + "\"")
      pexec.write("\t\"" + c + "\"")
      cexec.write("\t\"" + c + "\"")
      psize.write("\t\"" + c + "\"")
      csize.write("\t\"" + c + "\"")
    plat.write("\n")
    clat.write("\n")
    pexec.write("\n")
    cexec.write("\n")
    psize.write("\n")
    csize.write("\n")

    for s in servers:
      plat.write(s)
      clat.write(s)
      pexec.write(s)
      cexec.write(s)
      psize.write(s)
      csize.write(s)
      for client in clients:
        #c = str(int(client) * int(s))
        c = client
        process_client(path + "/" + wper + "_" + s + "_" + c, plat, clat, psize, csize)
        process_server(path + "/server_" + wper + "_" + s + "_" + c, pexec, cexec)
      plat.write("\n")
      clat.write("\n")
      pexec.write("\n")
      cexec.write("\n")
      psize.write("\n")
      csize.write("\n")
    plat.close()
    clat.close()
    pexec.close()
    cexec.close()
    psize.close()
    csize.close()
