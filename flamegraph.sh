export perf_dir="./local/perf"
sudo mkdir -p ${perf_dir}
echo "================recording...================"
sudo perf record -F 99 -a -g -o ${perf_dir}/perf.data -- sleep 300
# echo "================recording pid" $1 "...================"
# sudo perf record -F 99 -a -g -p $1 -o ${perf_dir}/perf.data -- sleep 300
sudo chmod 777 -R ${perf_dir}
echo "================scirpting================"
perf script -i ${perf_dir}/perf.data > ${perf_dir}/perf.out
echo "================stackcollapsing================"
./FlameGraph/stackcollapse-perf.pl ${perf_dir}/perf.out > ${perf_dir}/perf.folded
echo "================generating svg================"
./FlameGraph/flamegraph.pl ${perf_dir}/perf.folded > ${perf_dir}/flamegraph.svg

