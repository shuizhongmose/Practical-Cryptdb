#!/bin/bash

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

## Step1. chose jemalloc or tcmalloc
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

run_without_nohup() {
    mysql-proxy --defaults-file=./mysql-proxy.cnf --proxy-lua-script=`pwd`/wrapper.lua
}

run_with_nohup(){
    nohup mysql-proxy --defaults-file=./mysql-proxy.cnf --proxy-lua-script=`pwd`/wrapper.lua > server.log 2>&1 &
}

# step2. set OPE_CACHE_SIZE
echo "Set OPE_CACHE_SIZE, default is 10000:"
read -p "Enter choice [1000-20000], now 10000 is best: " choice

if [[ "$choice" =~ ^[0-9]+$ ]]; then
  if (( choice >= 1000 && choice <= 10000 )); then
    echo "Valid choice: $choice"
  else
    echo "Choice is out of the valid range [1000-10000]. Using default value 10000."
    choice=10000
  fi
else
  echo "Invalid input. Please enter an integer between 1000 and 10000. Using default value 10000."
  choice=10000
fi
export OPE_CACHE_SIZE=$choice
echo "OPE_CACHE_SIZE is set to $OPE_CACHE_SIZE"

# step2. mkdir
mkdir shadow

# step3. run
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
    echo "Run with default choice 1: run_without_nohup"
    run_without_nohup
    ;;
esac