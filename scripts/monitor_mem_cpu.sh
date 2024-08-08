#!/bin/bash

# 设置日志文件路径
LOG_FILE="cpu_mem_monitor.log"

# 设置收集信息的时间间隔，单位为秒
INTERVAL=60

# 创建日志文件，如果已经存在则追加内容
touch "$LOG_FILE"

# 开始循环，每隔一定时间收集一次信息
while true; do
  # 不存在mysql-proxy 则退出
  if ! pgrep -x "sysbench" > /dev/null; then
    exit
  fi
  # 获取当前日期和时间
  echo "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')" >> "$LOG_FILE"
  
  # 循环处理每个需要监控的进程
  for process in mysql-proxy sysbench mysqld; do
    echo "Process: $process" >> "$LOG_FILE"
    
    # 使用ps命令获取进程的CPU和内存使用率
    ps aux | grep $process | grep -v grep | awk '{cpu+=$3; mem+=$4} END {print "    CPU Usage: "cpu"%\n    Memory Usage: "mem"%"}' >> "$LOG_FILE"
  done

  # 使用free命令获取内存使用情况，并计算使用百分比
  MEM_TOTAL=$(free | grep Mem: | awk '{print $2}')
  MEM_USED=$(free | grep Mem: | awk '{print $3}')
  MEM_PERCENT=$(echo "scale=2; $MEM_USED*100/$MEM_TOTAL" | bc)
  echo "Total Memory Usage: $MEM_PERCENT%" >> "$LOG_FILE"
  
  # 输出分隔符，便于区分每次的记录
  echo "----------------------------------------" >> "$LOG_FILE"
  
  # 等待一定时间间隔后再次收集信息
  sleep "$INTERVAL"

done
