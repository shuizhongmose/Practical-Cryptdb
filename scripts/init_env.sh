#!/bin/bash
# set -e

# -9 是立即停止，不会给程序机会去执行任何清理代码
sudo pkill -15 sysbench
sudo pkill -15 mysql-proxy

sudo netstat -pntl | grep 3307

sleep 2

mysql -uroot -p -e "DROP DATABASE IF EXISTS \`sbtest\`;" \
                 -e "DROP DATABASE IF EXISTS \`encrypted_db\`;" \
                 -e "DROP DATABASE IF EXISTS \`remote_db\`;" \
                 -e "DROP DATABASE IF EXISTS \`cryptdb_udf\`;" \
                 -e "SHOW DATABASES;"

echo `rm shadow -rf`