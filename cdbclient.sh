#!/bin/bash

# 获取第一个参数
command=$1
dir=`pwd`

if [ "$command" = "initdb" ]; then
    mysql -h 127.0.0.1 -P3307 -uroot -p -e "CREATE DATABASE \`sbtest\` DEFAULT CHARACTER SET utf8;" -e "show databases;"
    sleep 2;
    cd $SYSBENCH && sysbench --oltp-tables-count=5 --oltp_table_size=0 --mysql-host=127.0.0.1 \
        --mysql-port=3307 --mysql-user=root --mysql-password=letmein \
        tests/include/oltp_legacy/insert.lua prepare
fi

cd $dir

`pwd`/mysql-src/build/client/mysql -uroot -pletmein -h 127.0.0.1 -P3307
