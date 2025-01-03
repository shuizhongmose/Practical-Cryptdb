# 使用Ubuntu 12.04作为基础镜像
FROM ubuntu:12.04
SHELL ["/bin/bash", "-c"]

ARG DEBIAN_FRONTEND=noninteractive

# 设置工作路径
WORKDIR /cryptdb
COPY . .

# 更换源地址，1204不受支持了，相关源被移动到了old-releases.ubuntu.com域名下
RUN sed -i.bak -r 's/(archive|security).ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list && \
    # 设置mysql的默认密码
    echo "mysql-server mysql-server/root_password password letmein" | debconf-set-selections && \
    echo "mysql-server mysql-server/root_password_again password letmein" | debconf-set-selections && \
    # 安装依赖，更新软件包列表
    apt-get -y update && \
    apt-get install -y software-properties-common && \
    apt-get install -y net-tools && \
    # 安装MySQL 5.5
    apt-get install -y mysql-server-5.5 && \
    # 为MySQL设置正确的绑定
    sed -i 's/^bind-address\s*=\s*127.0.0.1/bind-address = 0.0.0.0/' /etc/mysql/my.cnf && \
    mysql -V && \
    # 安装CryptDB需要的环境
    echo =============Install environment================================ && \
    apt-get install python-software-properties -y && \
    add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
    apt-get -y update && \
    apt-get remove --purge -y gcc g++ && \
    apt-get install gcc-4.7 g++-4.7 gawk liblua5.1-0-dev libntl-dev libmysqlclient-dev libssl-dev libbsd-dev libevent-dev libglib2.0-dev libgmp-dev mysql-server libaio-dev automake gtk-doc-tools flex cmake libncurses5-dev make ruby lua5.1 libmysqld-dev exuberant-ctags cscope -y && \
    ln -s /usr/bin/g++-4.7 /usr/bin/g++ && \
    ln -s /usr/bin/gcc-4.7 /usr/bin/gcc && \
    # install bison
    apt-get remove bison libbison-dev -y && \
    apt-get upgrade -y && \
    cd packages && pwd && dpkg -i libbison-dev_2.7.1.dfsg-1_amd64.deb && dpkg -i bison_2.7.1.dfsg-1_amd64.deb && cd .. && \
    # 清理部分步骤，如删除不必要的包
    apt-get autoremove -y && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# ARG CACHEBUST=1
RUN chmod -R 765 /cryptdb/ && \
    # 编译
    chmod +x *.sh && \
    sed -i "s/sudo //g" *.sh && \
    sed -i "s/-xvf/-xf/g" INSTALL.sh
    # # ubuntu1204 最新的g++版本是4.6
    # sed -i "s/g++-4.7/g++/g" INSTALL.sh && \
    # sed -i "s/g++-4.7/g++/g" Makefile

# 新的阶段用于构建MYSQL环境
RUN echo =============COMPILE MYSQL================================ && \
    rm -rf mysql-src && \
    tar -xf packages/mysql-src.tar.gz  && \
    export CXX=g++-4.7 && \
    g++ --version && \
    gcc --version && \
    cmake --version && \
    cd mysql-src && mkdir build && cd build  && \
    cmake -DWITH_EMBEDDED_SERVER=on -DENABLE_DTRACE=off ..   && \
    make && cd ../..

RUN echo =============INSTALL MYSQL-proxy========================= && \
    tar -xvf packages/mysql-proxy-0.8.5.tar.gz -C mysql-src/ && \
    binpath=`pwd`/mysql-src/mysql-proxy-0.8.5/bin  && \
    echo " " >> ~/.bashrc  && \
    echo PATH='$'PATH:${binpath} >> ~/.bashrc  && \
    source ~/.bashrc

RUN echo =============INSTALL Cryptdb============================= && \
    make && \
    make install && \
    chmod 0660 mysql-proxy.cnf && \
    echo ============Enjoy it!!!=====================================

RUN echo ######################################################## && \
    # 删除代码
    rm -rf .git && \
    rm -rf *.log && \
    # 删除所有临时编译文件.o, .a, .tmp
    find ./ -type f | grep \\.o$ | xargs rm -rf && \
    find ./ -type f | grep \\.a$ | xargs rm -rf && \
    find ./ -type f | grep \\.tmp$ | xargs rm -rf

ENV PATH="/cryptdb/mysql-src/mysql-proxy-0.8.5/bin:${PATH}"

# # # 启动mysqld
# CMD [ "mysqld" ]