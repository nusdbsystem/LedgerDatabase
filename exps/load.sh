let folder=$1

rm -f shard*.config
if [ $# -eq 1 ]; then
    python gen_conf.py $1
else
    python gen_conf.py $1 $2
fi
