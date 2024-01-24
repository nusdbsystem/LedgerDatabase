for host in `cat timeservers`
do
  echo timeserver $host
  ssh $host "pgrep timeserver"
done

for host in `cat replicas`
do
  echo server $host
  ssh $host "pgrep strongstore"
done

for host in `cat clients`
do
  echo client $host
  ssh $host "pgrep verifyClient"
done
