#!/bin/bash

CURR_DIR=`pwd`

Choose_tcmalloc(){
  echo "Do you use tcmalloc?:"
  read -p "Enter choice Y/N: " choice
  case $choice in
    'y')
      export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4
      ;;
    'Y')
      export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4
      ;;
    *)
      ;;
  esac
}

# STEP ONE: chose jemalloc or tcmalloc, or neither
echo "Do you use jemalloc?:"
read -p "Enter choice Y/N: " choice
case $choice in
  'y')
    export LD_PRELOAD=/usr/lib/libjemalloc.so.2
    ;;
  'Y')
    export LD_PRELOAD=/usr/lib/libjemalloc.so.2
    ;;
  *)
    Choose_tcmalloc
    ;;
esac

init_mysql_db(){
    mysql -h 127.0.0.1 -P3307 -uroot -p -e "CREATE DATABASE \`sbtest\` DEFAULT CHARACTER SET utf8;" -e "show databases;"
    sleep 2;
    cd $SYSBENCH && sysbench --oltp-tables-count=5 --oltp_table_size=0 --mysql-host=127.0.0.1 \
        --mysql-port=3307 --mysql-user=root --mysql-password=letmein \
        tests/include/oltp_legacy/insert.lua prepare
}

# STEP TWO: init db or not?
echo "Do you need to init db?:"
echo "including create db sbtest and new 5 empty tables for sysbench:"
read -p "Enter choice Y/N: " choice
case $choice in
  'y')
    init_mysql_db
    ;;
  'Y')
    init_mysql_db
    ;;
  *)
    ;;
esac

# STEP THREE: run client
echo "Now, connect to cryptdb server....."
cd $dir

${CURR_DIR}/mysql-src/build/client/mysql -uroot -pletmein -h 127.0.0.1 -P3307
