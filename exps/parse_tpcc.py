import sys

def process(infile, outfile, eol):
  with open(infile, "r") as fp:
    for line in fp:
      tps = float(line)
      outfile.write(str(tps) + eol)
      break

def print_result(fp, path, wper, servers, clients, t, lineno):
  fp.write("\"#Servers\"")
  for c in clients:
    fp.write("\t\"" + c + "\"")
  fp.write("\n")

  for s in servers:
    fp.write(s)
    for c in clients:
      infile = open(path + "/" + wper + "_" + s + "_" + c + "_" + t, "r")
      data = infile.read().splitlines()
      fp.write("\t\"" + data[lineno] + "\"")
    fp.write("\n")

path = sys.argv[1]
wpers = sys.argv[2].split(",")
servers = sys.argv[3].split(",")
clients = sys.argv[4].split(",")
thetas = sys.argv[5].split(",")

for wper in wpers:
  for t in thetas:
    ftps = open(path + "/tps_" + wper + "_" + t, "w")
    flat = open(path + "/lat_" + wper + "_" + t, "w")
    abort = open(path + "/abort_" + wper + "_" + t, "w")
    no = open(path + "/no", "w")
    pm = open(path + "/pm", "w")
    os = open(path + "/os", "w")
    dl = open(path + "/dl", "w")
    sl = open(path + "/sl", "w")

    print_result(ftps, path, wper, servers, clients, t, 0)
    print_result(flat, path, wper, servers, clients, t, 1)
    print_result(abort, path, wper, servers, clients, t, 4)
    print_result(no, path, wper, servers, clients, t, 5)
    print_result(pm, path, wper, servers, clients, t, 6)
    print_result(os, path, wper, servers, clients, t, 7)
    print_result(dl, path, wper, servers, clients, t, 8)
    print_result(sl, path, wper, servers, clients, t, 9)

    ftps.close()
    flat.close()
    abort.close()
    no.close()
    pm.close()
    os.close()
    dl.close()
    sl.close()
