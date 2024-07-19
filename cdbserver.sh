mkdir shadow
# mysql-proxy --defaults-file=./mysql-proxy.cnf --proxy-lua-script=`pwd`/wrapper.lua
nohup mysql-proxy --defaults-file=./mysql-proxy.cnf --proxy-lua-script=`pwd`/wrapper.lua > server.log 2>&1 &