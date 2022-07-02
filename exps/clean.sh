. env.sh

for server in `cat replicas`
do
  echo ${server}
  rsync -a ${expdir} ${server}:${rootdir}/
  ssh ${server} "killall -9 strongstore; rm -rf ${logdir}; mkdir -p /tmp/testdb; mkdir -p /tmp/testledger; rm -rf /tmp/testdb/*; rm -rf /tmp/testledger/*; rm -rf /tmp/qldb.store/"
done

for client in `cat clients`
do
  echo ${client}
  rsync -a ${expdir} ${client}:${rootdir}/
  ssh ${client} "killall -9 verifyClient; rm -rf ${logdir};"
done

for host in `cat timeservers`
do
  echo ${host}
  rsync -a ${expdir} ${host}:${rootdir}/
  ssh ${host} "killall -9 timeserver; rm -rf ${logdir};"
done
