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
            --threshold=0 \
            --pages-as-heap=no \
            ./obj/main/change_test > change_test.log 2>&1 &
}


echo "Choose the action to perform:"
echo "1) run_without_nohup"
echo "2) run_with_nohup"
echo "3) Run with Valgrind"
echo "4) Run with Valgrind + GDB"
echo "5) Run with Valgrind + Massif"
read -p "Enter choice (1, 2, 3, 4, 5): " choice

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
  *)
    echo "Invalid choice. Exiting."
    exit 1
    ;;
esac
