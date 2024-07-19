# 升级 GCC 到4.8并安装环境
NTL 版本（11.5.1）需要现代的 `C++11` 特性，而 `g++ 4.7` 对 `C++11` 的支持不完全，因此需要使用 `g++ 4.8` 或更高版本。为了兼容当前cryptdb，先尝试使用`g++4.8`。

```bash
# 设置 LD_LIBRARY_PATH
source setup.sh

sudo apt-get update -y
sudo apt-get remove bison libbison-dev -y
sudo apt-get purge bison libbison-dev -y
sudo apt-get autoremove -y
sudo apt-get upgrade -y

####################################################################
sudo apt-get install gcc-4.8 g++-4.8 -y
sudo rm /usr/bin/g++
sudo rm /usr/bin/gcc
sudo ln -s /usr/bin/g++-4.8 /usr/bin/g++
sudo ln -s /usr/bin/gcc-4.8 /usr/bin/gcc
####################################################################
# install environments patch
sudo apt-get install gawk liblua5.1-0-dev libntl-dev libmysqlclient-dev libssl-dev libbsd-dev libglib2.0-dev libgmp-dev mysql-server libaio-dev automake gtk-doc-tools flex cmake libncurses5-dev make ruby lua5.1 libmysqld-dev exuberant-ctags cscope -y

####################################################################
### 卸载 libevent-dev
sudo apt-get remove libevent-dev -y
sudo apt-get purge libevent-dev -y
sudo apt-get autoremove -y

### install libevent from source
wget https://github.com/libevent/libevent/releases/download/release-2.0.22-stable/libevent-2.0.22-stable.tar.gz

tar xf libevent-2.0.22-stable.tar.gz
cd libevent-2.0.22-stable
make clean
./configure --prefix=/usr
make
sudo make install
sudo ldconfig
ll /usr/lib | grep event

####################################################################
# install m4
sudo apt-get install m4 -y

####################################################################
cd Practical-Cryptdb
cd packages;sudo dpkg -i libbison-dev_2.7.1.dfsg-1_amd64.deb; sudo dpkg -i bison_2.7.1.dfsg-1_amd64.deb; cd ..

####################################################################
sudo apt-get install build-essential -y
```



# INSTALL GMP-6.2.1

GMP使用的是`gnu99`库，所以gcc 4.6-4.8都可以编译成功。

参考：[How to compile GMP from source?](https://gmplib.org/list-archives/gmp-discuss/2012-May/005042.html)

```bash
# 卸载 libgmp-dev
sudo apt-get remove libgmp-dev -y
sudo apt-get purge libgmp-dev -y
sudo apt-get autoremove -y

# install from source
wget https://gmplib.org/download/gmp/gmp-6.2.1.tar.xz
tar -xf gmp-6.2.1.tar.xz
cd gmp-6.2.1
### 安装到/usr/lib下面
./configure --prefix=/usr
make  
sudo make install
sudo ldconfig
ll /usr/lib | grep gmp
```



# INSTALL NTL-11.5.1

参考：

- [A Tour of NTL: Obtaining and Installing NTL for UNIX](https://libntl.org/doc/tour-unix.html)
- [NTL download page](https://libntl.org/download.html)

```bash
# 检查 LD_LIBRARY_PATH，如果没有配置全局变量，不然GMP会有版本冲突。GNU有默认的 GMP-5.0.2 
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH

# remove libevent-dev
sudo apt-get remove libntl-dev -y
sudo apt-get purge libntl-dev -y
sudo apt-get autoremove -y

wget https://libntl.org/ntl-11.5.1.tar.gz
tar -xzvf ntl-11.5.1.tar.gz  
cd ntl-11.5.1
cd src  
export CXX=g++-4.8
make clobber
./configure PREFIX=/usr GMP_PREFIX=/usr/lib SHARED=on 
make
make check # 可选
sudo make install
sudo ldconfig
ll /usr/lib | grep ntl
ldd /usr/lib/libntl.so # 会发现调用了 /usr/lib/libgmp.so.10
```

**错误记录：**

- **mysql启动错误**

**错误描述**：`Jul 18 19:06:57 ubuntu1204-16c32G kernel: [ 1060.684120] type=1400 audit(1721300817.492:167): apparmor="DENIED" operation="open" parent=1 profile="/usr/sbin/mysqld" name="/usr/local/lib/libntl.so.44.0.1" pid=3708 comm="mysqld" requested_mask="r" denied_mask="r" fsuid=106 ouid=1000` 

**解决方案**：NLT 手动安装后mysql 无法访问`/usr/local/lib`，所以在配置的时候使用`PREFIX`设置默认安装路径。

- 日志查看

```bash
tail -f /var/log/syslog
tail -f /var/log/mysql/error.log
```

<font color='red'>注意：GMP和NTL安装后，cryptdb会默认链接GNU的gmp库。所以`setup.sh`中配置了`LD_LIBRARY_PATH`变量。</font>



# COMPILE MYSQL-SRC

```bash
cd Practical-Cryptdb
rm -rf mysql-src
tar -xvf packages/mysql-src.tar.gz
export CXX=g++-4.8
cd mysql-src;mkdir build;cd build;cmake -DWITH_EMBEDDED_SERVER=on -DENABLE_DTRACE=off .. make
cd ../..
```



# INSTALL MYSQL-PROXY

```bash
tar -xvf packages/mysql-proxy-0.8.5.tar.gz -C mysql-src/

binpath=`pwd`/mysql-src/mysql-proxy-0.8.5/bin
echo $binpath

echo " " >> ~/.bashrc
echo PATH='$'PATH:${binpath} >> ~/.bashrc
source ~/.bashrc
```



# INSTALL and RUN Practical-Cryptdb

参考：

- [How to install libevent on Debian/Ubuntu/Centos Linux?](https://geeksww.com/tutorials/operating_systems/linux/installation/how_to_install_libevent_on_debianubuntucentos_linux.php)
- [libevent download page](https://libevent.org/)

```bash
make clean
make
sudo make install
chmod 0660 mysql-proxy.cnf
```

