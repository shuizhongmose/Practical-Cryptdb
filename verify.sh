mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tpcc1000"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb2"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb3"
rm -rf ./shadow/
mkdir ./shadow

run_without_nohup() {
    ##running this twice will caurse troubles because of the metadata mismatch
    ./obj/main/change_test
}

run_with_nohup() {
    #### 后台运行
    nohup ./obj/main/change_test > change_test.log 2>&1 &
}

## Valgrind 监控内存
run_with_valgrind() {
    rm valgrind_output.log -rf
    valgrind --tool=memcheck \
            --track-origins=yes \
            --log-file=valgrind_output.log \
            --verbose \
            --num-callers=50 \
            --show-reachable=yes \
            --leak-check=full \
            --show-leak-kinds=all \
            --max-stackframe=1000000 \
            ./obj/main/change_test > change_test.log 2>&1
}

## 如果用Valgrind + GDB 调试
run_with_valgrind_gdb() {
    rm valgrind_output.log -rf
    valgrind --tool=memcheck \
            --vgdb=yes \
            --vgdb-error=0 \
            --track-origins=yes \
            --log-file=valgrind_output.log \
            --verbose \
            --num-callers=50 \
            --show-reachable=yes \
            --leak-check=full \
            --show-leak-kinds=all \
            --max-stackframe=1000000 \
            ./obj/main/change_test > change_test.log 2>&1
}

## 使用valgrind+massif分析
run_with_valgrind_massif(){
    nohup valgrind --time-stamp=yes \
            --tool=massif \
            --massif-out-file=massif.out \
            --time-unit=B \
            --detailed-freq=1 \
            --threshold=0.5 \
            --max-snapshots=100 \
            ./obj/main/change_test > change_test.log 2>&1 &
}

## use kcg file
## 使用valgrind+massif分析
run_with_callgrind (){
  nohup valgrind --time-stamp=yes \
      --tool=callgrind \
      --callgrind-out-file=xtmemory.kcg \
      --dump-line=yes \
      --dump-instr=yes \
      --compress-strings=yes \
      --compress-pos=yes \
      --instr-atstart=yes \
      --collect-atstart=yes \
      --collect-jumps=yes \
      --collect-bus=yes \
      --collect-systime=yes \
      --cache-sim=yes \
      --branch-sim=yes \
      ./obj/main/change_test > change_test.log 2>&1 &

}

## use heaptrack
## 使用valgrind+massif分析
run_with_heaptrack (){
  nohup heaptrack ./obj/main/change_test > change_test.log 2>&1 &

}

echo "Choose the action to perform:"
echo "1) run_without_nohup"
echo "2) run_with_nohup"
echo "3) Run with Valgrind default"
echo "4) Run with Valgrind + GDB"
echo "5) Run with Valgrind + Massif"
echo "6) Run with valgrind + callgrind, can be analysised by kcachegrind"
echo "7) Run with heaptrack"
read -p "Enter choice (1, 2, 3, 4, 5, 6, 7): " choice

case $choice in
  1)
    run_without_nohup
    ;;
  2)
    run_with_nohup
    ;;
  3)
    run_with_valgrind
    ;;
  4)
    run_with_valgrind_gdb
    ;;
  5)
    run_with_valgrind_massif
    ;;
  6)
    run_with_callgrind
    ;;
  7)
    run_with_heaptrack
    ;;
  *)
    echo "Invalid choice. Exiting."
    exit 1
    ;;
esac
