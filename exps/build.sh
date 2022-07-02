if [ $(echo "$1" | awk '{print tolower($0)}') == 'qldb' ]
then
  flag=OFF
else
  flag=ON
fi

. env.sh

for host in `cat clients timeservers replicas`
do
  echo ${host}
  ssh ${host} "source ~/.profile; source ~/.bashrc; mkdir -p ${rootdir}/build; cd ${rootdir}/build; rm -rf *; cmake -DLEDGERDB=${flag} ..; make -j6;" &
done
