[TOC]
# 写在最前

<font color='red'>

1. NTL 版本（11.5.1）需要现代的 `C++11` 特性，而 `g++ 4.7` 对 `C++11` 的支持不完全，因此需要使用 `g++ 4.8` 或更高版本。为了兼容当前cryptdb，先尝试使用`g++4.8`。
2. `mysql-src` 升级到 `5.5.60` 会导致 LEX接口错误，不可用。所以`mysql-server` 是`5.5.60`，`mysql-src`是`5.5.14`版本。
3. 使用`massif`测试时，发现`mysql_init()`会导致内存持续增加，所以升级`ubuntu`到`20.04`测试。测试没问题，同时使用`jemalloc`或者`tcmalloc`，TPS提升一倍（`tcmalloc`需要`libstdc++6 ≥ 6`）。
4. 查看`glibc`版本：`ldd --version`
5. 查看`libstdc++6`版本：
```bash
$ ls -l /usr/lib/x86_64-linux-gnu/libstdc++.so.*
lrwxrwxrwx 1 root root      19 Jul  9  2023 /usr/lib/x86_64-linux-gnu/libstdc++.so.6 -> libstdc++.so.6.0.28
-rw-r--r-- 1 root root 1956992 Jul  9  2023 /usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.28
```

 </font>

总结：

| mysql-src版本 | GCC 版本 | 能否运行 | 问题 |
| --- | --- | --- | --- |
| 5.5.14 | 4.8 | <font color='green'><b>✓</b></font> | 由于GCC 4.8 无法很好支持 c++11 的 thread_locals属性，引起了NTL在多线程下的内存泄露
| 5.5.14 | 4.9 |  | 因为 GCC 4.9对c++标准的更严格遵循，所以代码中通过`*rob`获取类中`protected`或者`private`变量的方式不可行。可以修改mysql-src 5.5.14源码，支持对现有私有变量的访问GET/SET操作
| 5.5.60 | 4.9 | <font color='red'><b>×</b></font>  | 编译mysql-src时没有任何警告提示。但是需要大量修改 LEX 相关的代码，5.5.60 版本的lex接口与 5.5.14 的很不一样


# 安装必要环境

## 安装GCC 4.8

> [How to install gcc-4.8](https://askubuntu.com/questions/271388/how-to-install-gcc-4-8)
> [ubuntu20.04版本安装gcc-4.8](https://blog.csdn.net/feinifi/article/details/121793945)

```bash
sudo apt-get update

sudo apt-get remove bison libbison-dev
sudo apt-get purge bison libbison-dev
sudo apt-get autoremove

#### 调用 NTL::RandomLen时会有内存泄露问题
### 因为GCC 4.8 对c++11的thread_local支持不是很好
###### 这种情况下，编译NTL时需要设置 NTL_THREADS=on 和 NTL_TLS_HACK=on，避免直接依赖 C++11 标准中的 thread_local 关键字。
###### 这种变通方法的目的是确保在不完全支持 C++11 的编译器上，NTL 仍然可以正常运行，尤其是在涉及多线程和线程本地存储的情况下。

#### ubuntu 20.04默认不提供gcc4.8的包，所以需要提供源地址
sudo vi /etc/apt/sources.list
#### 在末尾添加
deb http://dk.archive.ubuntu.com/ubuntu/ xenial main
deb http://dk.archive.ubuntu.com/ubuntu/ xenial universe

sudo apt update

sudo apt-get install gcc-4.8 g++-4.8 -y
sudo rm /usr/bin/g++
sudo rm /usr/bin/gcc
sudo ln -s /usr/bin/g++-4.8 /usr/bin/g++
sudo ln -s /usr/bin/gcc-4.8 /usr/bin/gcc

## GCC 4.9，用于支持完整的 NTL（不可用）
sudo apt-get install gcc-4.9 g++-4.9 -y
sudo ln -s /usr/bin/g++-4.9 /usr/bin/g++
sudo ln -s /usr/bin/gcc-4.9 /usr/bin/gcc

# 确认版本
g++ -v
gcc -v
```

## 安装MySQL 5.5.60

### 安装mysql server 5.5.60 
> 参考我的有道云文档《[02.学习-工作相关/11.数据库/MySQL/04.ubuntu 16.04手动安装5.5.60](https://note.youdao.com/web/#/file/53F7F037769241E0970507F5BA6A3E46/markdown/WEB6fe61445e24c1ee2b2ba644dd8f9fe0a/)》

### 安装相应的开发包
- 从[这儿](https://launchpad.net/~ubuntu-security-proposed/+archive/ubuntu/ppa/+build/14781207) 下载`libmysqlclient18_5.5.60-0ubuntu0.14.04.1_amd64.deb `、`libmysqld-dev-5.5.60-0ubuntu0.14.04.1`和`libmysqlclient-dev-5.5.60-0ubuntu0.14.04.1`

- 其中`libmysqlclient18_5.5.60-0ubuntu0.14.04.1_amd64.deb`依赖`multiarch-support`，从[这儿](https://launchpad.net/~ubuntu-security-proposed/+archive/ubuntu/ppa/+build/16533842)下载`multiarch-support`（这个在ubuntu 20.04上是不需要的了）

```bash
sudo apt remove libmysqld-dev -y
sudo apt remove libmysqlclient-dev -y
sudo apt autoremove -y

wget https://launchpad.net/~ubuntu-security-proposed/+archive/ubuntu/ppa/+build/16533842/+files/multiarch-support_2.19-0ubuntu6.15_amd64.deb
wget https://launchpad.net/~ubuntu-security-proposed/+archive/ubuntu/ppa/+build/14781207/+files/libmysqlclient18_5.5.60-0ubuntu0.14.04.1_amd64.deb
wget https://launchpad.net/~ubuntu-security-proposed/+archive/ubuntu/ppa/+build/14781207/+files/libmysqlclient-dev_5.5.60-0ubuntu0.14.04.1_amd64.deb
wget https://launchpad.net/~ubuntu-security-proposed/+archive/ubuntu/ppa/+build/14781207/+files/libmysqld-dev_5.5.60-0ubuntu0.14.04.1_amd64.deb

sudo dpkg -i multiarch-support_2.19-0ubuntu6.15_amd64.deb
sudo dpkg -i libmysqlclient18_5.5.60-0ubuntu0.14.04.1_amd64.deb

sudo dpkg -i libmysqlclient-dev_5.5.60-0ubuntu0.14.04.1_amd64.deb

### 如果安装过程中提示缺少一些插件，执行下面这条语句后再执行dpkg -i操作
sudo apt --fix-broken install

sudo dpkg -i libmysqld-dev_5.5.60-0ubuntu0.14.04.1_amd64.deb
```

## 安装基础环境
```bash
sudo apt-get install gawk \
liblua5.1-0-dev \
libssl-dev \
libbsd-dev \
libglib2.0-dev \
libaio-dev \
automake \
gtk-doc-tools \
flex \
cmake \
libncurses5-dev \
make \
ruby \
lua5.1 \
exuberant-ctags \
cscope -y

## 如果出错执行下面的语句后再重新执行apt install
sudo apt --fix-broken install

sudo apt-get install m4 -y
sudo apt-get install libreadline-dev
```

## 安装`libbison`库
```bash
export CXX=g++-4.8

// 在 Practical-Cryptdb 目录下
cd Practical-Cryptdb/
cd packages;sudo dpkg -i libbison-dev_2.7.1.dfsg-1_amd64.deb; sudo dpkg -i bison_2.7.1.dfsg-1_amd64.deb; cd ..

sudo apt-get install build-essential -y

### 这一步安装后，GCC和G++版本会变成系统自带的版本，需要重新建立软连接
sudo rm /usr/bin/g++
sudo rm /usr/bin/gcc
sudo ln -s /usr/bin/g++-4.8 /usr/bin/g++
sudo ln -s /usr/bin/gcc-4.8 /usr/bin/gcc
```

## 安装OpenSSL 1.1.1f
```bash
#### 系统自带为1.1.1f
openssl version
```

## 安装 libevent
```bash
g++ -v
gcc -v

### 卸载 libevent-dev
sudo apt-get remove libevent-dev -y
sudo apt-get purge libevent-dev -y
sudo apt-get autoremove -y

### install libevent from source
wget https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz
tar xf libevent-2.1.12-stable.tar.gz
cd libevent-2.1.12-stable
make clean
export CXX=g++-4.8
./configure --prefix=/usr
make
sudo make install
sudo ldconfig
```

# INSTALL GMP-6.2.1

GMP使用的是`gnu99`库，所以gcc 4.6-4.9都可以编译成功。

参考：[How to compile GMP from source?](https://gmplib.org/list-archives/gmp-discuss/2012-May/005042.html)

```bash
g++ -v
gcc -v

# 卸载 libgmp-dev
sudo apt-get remove libgmp-dev
sudo apt-get purge libgmp-dev
sudo apt-get autoremove

# install from source
wget --no-check-certificate  https://gmplib.org/download/gmp/gmp-6.2.1.tar.xz
tar -xf gmp-6.2.1.tar.xz
cd gmp-6.2.1
### 安装到/usr/lib下面
export CXX=g++-4.8
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
./configure --prefix=/usr
make  
sudo make install
sudo ldconfig

### 卸载
sudo make uninstall
```

确定GMP源：==

```bash
// You should see files like libgmp.so or libgmp.a
ll /usr/lib | grep gmp

// you should see files like gmp.h
ll /usr/include | grep gmp

// show paths pointing to /usr/lib/libgmp.so
ldconfig -p | grep gmp

```

# INSTALL NTL-11.5.1

参考：[A Tour of NTL: Obtaining and Installing NTL for UNIX](https://libntl.org/doc/tour-unix.html)

```bash
g++ -v
gcc -v

# 配置全局变量，不然GMP会有版本冲突。GNU有默认的 GMP-5.0.2 
wget --no-check-certificate  https://libntl.org/ntl-11.5.1.tar.gz
tar -xzvf ntl-11.5.1.tar.gz  
cd ntl-11.5.1
cd src  

export CXX=g++-4.8
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH

make clean
make clobber

#### 1. 启用 NTL 的范围检查功能（调试时使用）设置：NTL_RANGE_CHECK=on（使用sysbench进行压测时，需要去掉该选项）
#### 2. 如果使用 Valgrind 调试时遇到 "unhandled instruction bytes" 错误，需要升级 Valgrind 至 3.9.0 版本，参考【手动安装valgrind 3.9.0】
#### 3. 对于 GCC 4.8 版本：由于 GCC 4.8 对 C++11 的 thread_local 支持不好，可能会导致 NTL 的接口函数出现 "still reachable" 错误，需要设置：NTL_THREADS=on NTL_TLS_HACK=off

./configure PREFIX=/usr GMP_PREFIX=/usr/lib SHARED=on NTL_RANGE_CHECK=on NTL_THREADS=on NTL_TLS_HACK=off

make
make check # 可选
sudo make install
sudo ldconfig
```

==其中手动安装valgrind 3.9.0资料在有道云笔记《08.开发语言学习/02.学习资料-工作/c++/03.内存调试》==

## 运行NTL测试代码
1. 可以直接使用`make check`运行所有的测试
2. 单独运行某个测试：
```bash
// run ZZTest.cpp
g++ -I../include -I. -g -std=c++11 -pthread -march=native -o .libs/ZZTest ZZTest.cpp  ./.libs/libntl.so -L/usr/lib/lib /usr/lib/libgmp.so -lpthread -pthread

// use valgrind + gdb to check memory leak
valgrind --vgdb=yes --vgdb-error=0 --log-file=valgrind_log.txt ./ZZTest

// open another shell to run the following commands
gdb ./ZZTest

// 设置断点
break main

//// in GDB, input:
(gdb) target remote | vgdb
```

## 安装NTL中可能出现的错误

###   mysql启动错误

**错误描述**：`Jul 18 19:06:57 ubuntu1204-16c32G kernel: [ 1060.684120] type=1400 audit(1721300817.492:167): apparmor="DENIED" operation="open" parent=1 profile="/usr/sbin/mysqld" name="/usr/local/lib/libntl.so.44.0.1" pid=3708 comm="mysqld" requested_mask="r" denied_mask="r" fsuid=106 ouid=1000`

**解决方案**：NLT 手动安装后mysql 无法访问`/usr/local/lib`

*   日志查看

```bash
tail -f /var/log/syslog
tail -f /var/log/mysql/error.log
```

# 安装tcmalloc（与jemalloc 二选一）
## 直接安装`tcmalloc`
- 参考：
    - [mysql占用内存不释放，并且一直缓慢增长](https://greatsql.cn/thread-167-1-1.html)
    - [浅谈MySQL数据库初始化和清理的三种方式，占用进程内存情况](https://blog.csdn.net/wjl990316fddwjl/article/details/135238852)
    - [TCMalloc：线程缓存 Malloc](https://goog-perftools.sourceforge.net/doc/tcmalloc.html)

```bash
sudo apt install google-perftools libgoogle-perftools-dev

### 检查是否安装上

ll /usr/lib/x86_64-linux-gnu/ | grep malloc  
dpkg -l | grep libgoogle-perftools-dev
```

## 手动安装`tcmalloc`(无用)

<font color='red'>

- <b>2024/12/9记录：仍然报错，`valgrind`就不能用`tcmalloc`</b>

直接安装`tcmalloc`会在使用`valgrind`内存测试时报`mismatch`的错误，解决方法参考：
- [Why does Memcheck report many "Mismatched free() / delete / delete []" errors when my code is correct?](https://valgrind.org/docs/manual/faq.html#faq.mismatches)

所以需要手动安装`google tcmalloc`库(2.9.1)，编译时添加选项`CPPFLAGS=-DTCMALLOC_NO_ALIASES`。

</font>

```bash
sudo apt-get remove libgoogle-perftools-dev
sudo apt autoremove

sudo apt-get install -y git automake libtool pkg-config build-essential libunwind-dev

git clone https://github.com/gperftools/gperftools.git
cd gperftools

git checkout gperftools-2.9.1

export CC=gcc-4.8
export CXX=g++-4.8
./autogen.sh
./configure CPPFLAGS=-DTCMALLOC_NO_ALIASES

make 
sudo make install
sudo ldconfig

### 确认已经安装
ldconfig -p | grep tcmalloc
#### 可以看到如下输出：
libtcmalloc.so.4 (libc6,x86-64) => /usr/local/lib/libtcmalloc.so.4
```

# 安装 `jemalloc`(与 `tcmalloc` 二选一)
```bash
cd /tmp
wget https://github.com/jemalloc/jemalloc/releases/download/5.3.0/jemalloc-5.3.0.tar.bz2

tar -xjf jemalloc-5.3.0.tar.bz2
cd jemalloc-5.3.0

export CC=gcc-4.8
export CXX=g++-4.8
./autogen.sh
./configure --prefix=/usr --libdir=/usr/lib --enable-static --enable-shared

make
sudo make install
sudo ldconfig

### 验证安装
ldconfig -p | grep jemalloc

ll /usr/lib | grep jemalloc
```

安装之后`jemalloc`在：`/usr/lib/libjemalloc.so.2`

# 修改MySQL配置
<font color='red'><b>使用`tcmalloc`或者`jemalloc`配置</b></font>
```bash
sudo vi /etc/my.cnf
### 增加以下两行
#### 如果使用的是tcmalloc库，则增加：
[mysqld_safe]
malloc-lib=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4

#### 如果使用的是jemalloc库，则增加：
[mysqld_safe]
malloc-lib=/usr/lib/libjemalloc.so.2
```
重启MySQL服务：
```bash
sudo /etc/init.d/mysql.server restart
```

# COMPILE MYSQL-SRC

```bash
g++ -v 
gcc -v

cd Practical-Cryptdb
rm -rf mysql-src
tar -xvf packages/mysql-src.tar.gz
cd mysql-src;mkdir build;cd build;

#### 有可能需要
sudo apt-get install cmake -y 
sudo apt install libncurses5-dev -y

export CXX=g++-4.8

# 非debug版本
### （1）不使用tcmalloc等
cmake -DWITH_EMBEDDED_SERVER=on -DENABLE_DTRACE=off ..
### （2）使用 jemalloc
cmake -DWITH_EMBEDDED_SERVER=on -DENABLE_DTRACE=off -DMALLOC_LIB=/usr/lib/libjemalloc.so.2 ..
### （3）使用tcmalloc
cmake -DWITH_EMBEDDED_SERVER=on -DENABLE_DTRACE=off -DMALLOC_LIB=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4 ..

###### cmake 之后有许多warning，可以忽略，继续

# debug版本，根据编译错误修改代码，有的有问题
# cmake -DWITH_EMBEDDED_SERVER=ON -DCMAKE_BUILD_TYPE=Debug -DWITH_DEBUG=1 \
# -DCMAKE_CXX_FLAGS="-Wno-unused-variable -Wno-unused-function -Wno-narrowing -Wno-unused-but-set-variable" \
# -DCMAKE_C_FLAGS="-Wno-unused-variable -Wno-unused-function -Wno-narrowing -Wno-unused-but-set-variable" ..

make
cd ../..
```

<font color="red"><b>如果使用debug版本需要修改的代码如下（没有成功）：</b></font>

*   `mysql-src/mysys/md5.c: 179`
    第 179 行代码为`memset(ctx, 0, sizeof(ctx));`，其中`ctx`是指针，此次有问题，应该修改为`memset(ctx, 0, sizeof(*ctx));`

*   `mysql-src/include/my_global.h: 393`
    将393 行代码替换为`(void) sizeof(char[(X) ? 1 : -1]);`，替换之后此部分代码如下：

```cpp
#ifndef __GNUC__
#define compile_time_assert(X)  do { } while(0)
#else
#define compile_time_assert(X)                                  \
  do                                                            \
  {                                                             \
    (void) sizeof(char[(X) ? 1 : -1]);                          \
  } while(0)
#endif
```

*   `mysql-src/dbug/dbug.c:1876`
    第1876行定义了`newfile`但是未使用，执行如下命令。定位到1905行，添加一行`(void)newfile;`告诉编译器我们知道 newfile 未被使用。修改后如下：

```bash
        newfile= !EXISTS(name);
        (void)newfile;
```

*   `log.h:351`
    将351、352行修改如下：

```cpp
  using MYSQL_LOG::generate_name;
  using MYSQL_LOG::is_open;
```

*   `pfs.cc:1508`
    在1578、1579行增加下面两行代码，告知编译器知道了

```cpp
 (void)last_writer;
 (void)last_reader;
```

*   `storage/myisam/mi_checksum.c:37`
    将 37 行代码

```cpp
      memcpy((char*) &pos, buf+rec->length- portable_sizeof_char_ptr,
             sizeof(char*));
```

修改为：

```cpp
memcpy((char*) &pos, buf + rec->length - portable_sizeof_char_ptr, portable_sizeof_char_ptr);
```

*   `storage/myisam/mi_key.c:421`
    421行修改为：

```cpp
      memcpy(record+keyseg->start+keyseg->bit_start,
             (char*) &blob_ptr,sizeof(blob_ptr));
```

*   `storage/myisam/mi_packrec.c:1054`
    1054行修改为：`memcpy((char*) to+pack_length, &bit_buff->blob_pos, length);`

*   `storage/innobase/btr/btr0cur.c:3535`
    3535行修改为：

```cpp
                field = rec_get_nth_field(rec, offsets, i, &rec_len);
                (void) field;
```

*   `storage/innobase/handler/ha_innodb.cc`
    搜索宏`MYSQL_SYSVAR_ULONG`调用的地方，将其中的`~0L`修改为`ULONG_MAX`。

```bash
:%s/\~0L/ULONG_MAX/g
```

*   `plugin/semisync/semisync.cc:30`

```cpp
const char  ReplSemiSyncBase::kSyncHeader[2] =
    {static_cast<char>(ReplSemiSyncBase::kPacketMagicNum), 0};
```

*   `plugin/semisync/semisync_master_plugin.cc`、`plugin/semisync/semisync_slave_plugin.cc`

```cpp
:%s/\~0L/ULONG_MAX/g
```

*   `unittest/mysys/lf-t.c:38`
    48行添加 `(void)x;`

```cpp
 46   for (x= ((int)(intptr)(&m)); m ; m--)
 47   {
 48     (void)x;
 49     lf_pinbox_put_pins(pins);
 50     pins= lf_pinbox_get_pins(&lf_allocator.pinbox);
 51   }
```

*   `tests/mysql_client_test.c:18599`

```cpp
18603   error= 0;
18604   (void)error;
```

*   `client/mysql.cc:1574`

```cpp
:%s/ULONG_MAX/static_cast<longlong>(ULONG_MAX)/g
```

*   `client/mysqlbinlog.cc:1171`

```cpp
1150    REQUIRED_ARG, (longlong)(~(my_off_t)0), BIN_LOG_HEADER_SIZE,
1151    (longlong)(~(my_off_t)0), 0, 0, 0},
```

*   `sql/field.cc:7381`

```cpp
7381       bmove(ptr+packlength,(char*) &from, length);
7898     bmove(ptr + packlength, (char*) &from, length);
```

*   `sql/item_func.cc:5087`

```cpp
5092   error= get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry);
5093   (void)error;
```

*   `sql/mysqld.cc` 5152、6107行

```cpp
5152   int ip_flags __attribute__((unused)) = 0, socket_flags __attribute__((unused)) = 0, flags = 0, retval;

```

# INSTALL MYSQL-PROXY

```bash
tar -xvf packages/mysql-proxy-0.8.5.tar.gz -C mysql-src/

# 下面的步骤执行一次即可
binpath=`pwd`/mysql-src/mysql-proxy-0.8.5/bin
echo $binpath

echo " " >> ~/.bashrc
echo PATH='$'PATH:${binpath} >> ~/.bashrc
source ~/.bashrc
```

# INSTALL Practical-Cryptdb

## 参考

- [How to install libevent on Debian/Ubuntu/Centos Linux?](https://geeksww.com/tutorials/operating_systems/linux/installation/how_to_install_libevent_on_debianubuntucentos_linux.php)
- [libevent download page](https://libevent.org/)

## 修改UDF 库的安装路径
1. 连接MySQL server
2. 在 MySQL 命令行中，运行以下命令查看插件目录的位置：
```sql
SHOW VARIABLES LIKE 'plugin_dir';
```
你会得到类似这样的输出：
```bash
mysql> SHOW VARIABLES LIKE 'plugin_dir';
+---------------+------------------------------+
| Variable_name | Value                        |
+---------------+------------------------------+
| plugin_dir    | /usr/local/mysql/lib/plugin/ |
+---------------+------------------------------+
1 row in set (0.00 sec)
```
3. 修改 `Makefile` 文件的`MYSQL_PLUGIN_DIR`目录(<font color='red'>修改一次即可</font>)

```shell
# install mysql 5.5.60 on ubuntu 16.04 mannully in /usr/local/mysql
UBUNTU_VERSION := $(shell lsb_release -rs)

ifeq ($(UBUNTU_VERSION), 12.04)
    MYSQL_PLUGIN_DIR := /usr/lib/mysql/plugin
else ifeq ($(UBUNTU_VERSION), 16.04)
    MYSQL_PLUGIN_DIR := /usr/local/mysql/lib/plugin
else
    $(warning Unsupported Ubuntu version, defaulting MYSQL_PLUGIN_DIR to /usr/lib/mysql/plugin)
    MYSQL_PLUGIN_DIR := /usr/lib/mysql/plugin
endif
```

## 编译和安装Practical-Cryptdb
```bash
# 设置 LD_LIBRARY_PATH，执行一次就行
source setup.sh

chmod 0660 mysql-proxy.cnf
chmod +x scripts/*.sh
chmod +x *.sh

### 运行编译&安装脚本，根据提示选择对应的编译选项
./make.sh 
```
然后就可以运行程序了。