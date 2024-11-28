mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tpcc1000"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb2"
mysql -uroot -pletmein -h127.0.0.1 -e "drop database if exists tdb3"
rm -rf ./shadow/
mkdir ./shadow

##running this twice will caurse troubles because of the metadata mismatch
# ./obj/main/change_test

#### 后台运行
# nohup ./obj/main/change_test > change_test.log 2>&1 &

## Valgrind 监控内存
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

## 如果用Valgrind + GDB 调试
# rm valgrind_output.log -rf
# valgrind --tool=memcheck \
#          --vgdb=yes \
#          --vgdb-error=0 \
#          --track-origins=yes \
#          --log-file=valgrind_output.log \
#          --verbose \
#          --num-callers=50 \
#          --show-reachable=yes \
#          --leak-check=full \
#          --show-leak-kinds=all \
#          --max-stackframe=1000000 \
#          ./obj/main/change_test > change_test.log 2>&1


