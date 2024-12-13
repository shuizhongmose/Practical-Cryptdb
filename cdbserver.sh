#!/bin/bash

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
    ;;
esac

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

mkdir shadow

run_without_nohup() {
    mysql-proxy --defaults-file=./mysql-proxy.cnf --proxy-lua-script=`pwd`/wrapper.lua
}

run_with_nohup(){
    nohup mysql-proxy --defaults-file=./mysql-proxy.cnf --proxy-lua-script=`pwd`/wrapper.lua > server.log 2>&1 &
}

echo "Choose the action to start cryptdb service:"
echo "1) run without nohup, log in shell"
echo "2) run nohup, log in server.log"
read -p "Enter choice (1, 2): " choice

case $choice in
  1)
    run_without_nohup
    ;;
  2)
    run_with_nohup
    ;;
  *)
    echo "Invalid choice. Exiting."
    exit 1
    ;;
esac