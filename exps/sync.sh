. env.sh

copy_from=${rootdir}
copy_to=${copy_from%/*}

for host in `cat timeservers replicas`
do
  echo $host
  rsync -a --exclude '*result' $copy_from $host:$copy_to
done

for host in `cat clients`
do
  echo $host
  rsync -a --exclude '*result' $copy_from $host:$copy_to
done
