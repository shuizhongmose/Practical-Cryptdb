#!/bin/bash


sudo pkill -9 sysbench
sudo pkill -9 mysql-proxy

sudo netstat -pntl | grep 3307

sleep 2

mysql -uroot -p -e "drop database \`cryptdb_udf\`;" -e "drop database \`remote_db\`;" -e "drop database \`sbtest\`;" -e "show databases;"

echo `rm shadow -rf`