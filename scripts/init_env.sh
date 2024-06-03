#!/bin/bash
# set -e

sudo pkill -9 sysbench
sudo pkill -9 mysql-proxy

sudo netstat -pntl | grep 3307

sleep 2

mysql -uroot -p -e "DROP DATABASE IF EXISTS \`sbtest\`;" \
                 -e "DROP DATABASE IF EXISTS \`encrypted_db\`;" \
                 -e "DROP DATABASE IF EXISTS \`remote_db\`;" \
                 -e "DROP DATABASE IF EXISTS \`cryptdb_udf\`;" \
                 -e "SHOW DATABASES;"

echo `rm shadow -rf`