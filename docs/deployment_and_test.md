[TOC]

# 一、测试环境

*   sysbench 1.0（0.5执行超过929就会出错）
*   mysql 5.5.62
*   ubuntu 1204 虚拟机，10核，8G内存

# 二、参考文献

*   [官方文档](https://css.csail.mit.edu/cryptdb/)
*   [ MySQL Standards Compliance](https://dev.mysql.com/doc/refman/8.0/en/compatibility.html)
*   Skiba M, Mainka M S C, Mladenov D I V, et al. Bachelor Thesis Analysis of Encrypted Databases with CryptDB[J]. 2015. <http://dx.doi.org/10.1007/978-981-10-8633-5_31>
*   [Sysbench 使用总结](https://cloud.tencent.com/developer/article/1811159)

# 三（1）、安装 cryptdb

1.  运行 `scripts/install.rb .` ，在所有`install`的位置，加上`sudo`，这样不用`root`权限运行了
    *   根据日志结果，如果只是`cryptdb`没有安装成功，则每次只需要在`cryptdb`目录下执行`make`和`sudo make install`即可。
2.  在 `~/.bashrc`中添加`export EDBDIR=/path/to/cryptdb`，然后执行`source ~/.bashrc`
3.  启动`mysql-proxy`

```bash
nohup mysql-proxy --plugins=proxy --event-threads=10 \
--max-open-files=1024 --proxy-lua-script=$EDBDIR/mysqlproxy/wrapper.lua \
--proxy-address=127.0.0.1:3307  --proxy-backend-addresses=localhost:3306 > mysql-proxy.log &
```

==启动之后需要等一会，不然文件还没有创建完。所以日志部分还需要改进==

# 三（2）、安装Practical-Cryptdb

## 1.  脚本安装

```bash
# update apt source
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bk
sudo vi /etc/apt/sources.list

## add lines
deb http://old-releases.ubuntu.com/ubuntu/ precise main restricted universe multiverse
deb http://old-releases.ubuntu.com/ubuntu/ precise-updates main restricted universe multiverse
deb http://old-releases.ubuntu.com/ubuntu/ precise-security main restricted universe multiverse

sudo apt-get update

# install mysql server，密码设置为 letmein
sudo apt-get install mysql-server-5.5 -y

# install client
sudo apt-get install mysql-client-core-5.5 -y

# install cryptdb environment
./INSTALL.sh
source ~/.bashrc
source setup.sh
```

## 2.  手动安装`g++-4.7` & `gcc-4.7`
如果脚本安装出错，且`g++-4.7`无法找到源，则需要我们手动安装

- 参考安装方法在[这儿](https://blog.swiftsoftwaregroup.com/upgrade-gcc-4-7-ubuntu-12-04/#install)

```bash
sudo apt-get install build-essential

sudo apt-get install python-software-properties
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install gcc-4.7 g++-4.7

# install 4.6 and 4.7, and set 4.7 as default
sudo update-alternatives --remove gcc /usr/bin/gcc-4.6
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.7 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.7
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.6 40 --slave /usr/bin/g++ g++ /usr/bin/g++-4.6
```

# 四（1）、基本功能测试

==以`Practical-CryptDB`项目为例进行测试。==

## 1.  启动测试服务

```bash
# start mysql-proxy server
./cdbserver.sh

# start cleint
./cdbclient.sh 
```

## 2. 基本功能 SQL

```sql
CREATE DATABASE `sbtest` DEFAULT CHARACTER SET utf8;
use sbtest;
CREATE TABLE user (
  id INT AUTO_INCREMENT PRIMARY KEY,
  name TEXT,
  age INTEGER
) ENGINE=InnoDB;
INSERT INTO user (name, age) VALUES ('alice', 19), ('bob', 20), ('chris', 21);
select * from user;

-- 下面的 SQL 会触发AdjustOnion

select sum(greatest(age,20)) from user;
select * from user order by age desc;

--- oPLAIN 操作，出发SEARCH onion生成

select * from user where name = 'alice'; 

-- ⭐下面的测试需要设置在插入之前全局设置`SECURE_CRYPTDB=false`。
SELECT * FROM user WHERE name LIKE '%alice%';

---- 测试 group by 功能
```
## 3. 使用`tutorial-basic.lua`测试mysql-proxy性能
```bash
# 启动 mysql-proxy 服务端
mkdir shadow
mysql-proxy --defaults-file=./mysql-proxy.cnf --proxy-lua-script=`pwd`/tutorial-basic.lua

# 启动客户端
`pwd`/mysql-src/build/client/mysql -uroot -pletmein -h 127.0.0.1 -P3307

# 后面可以参考sysbench的压测
```

==使用5个表，每个表插入1w数据，24线程。得到的性能如下：==
```bash
SQL statistics:
    queries performed:
        read:                            0
        write:                           50000
        other:                           0
        total:                           50000
    transactions:                        50000  (1859.42 per sec.)
    queries:                             50000  (1859.42 per sec.)
    ignored errors:                      0      (0.00 per sec.)
    reconnects:                          0      (0.00 per sec.)

General statistics:
    total time:                          26.8861s
    total number of events:              50000

Latency (ms):
         min:                                    0.77
         avg:                                   12.89
         max:                                  234.29
         95th percentile:                       33.12
         sum:                               644716.56

Threads fairness:
    events (avg/stddev):           2083.3333/11.00
    execution time (avg/stddev):   26.8632/0.01
```

# 四（2）、sysbench 基准测试

==以`Practical-CryptDB`项目为例进行测试。==

## 安装 sysbench 1.0

```bash
git clone https://github.com/akopytov/sysbench.git
cd sysbench
git checkout 1.0  # switch to the 1.0 branch if available

./autogen.sh
./configure
make
sudo make install
```

## 测试流程

```bash
# 登录mysql-proxy
mysql -u root -pletmein -h 127.0.0.1 -P 3307

##### 创建测试数据库
CREATE DATABASE `sbtest` DEFAULT CHARACTER SET utf8;

# 新开一个shell，执行如下的测试命令
cd /path/to/sysbench-1.0

##### 创建5个空表
sysbench --db-driver=mysql --oltp-tables-count=5 --oltp_table_size=0 --mysql-host=127.0.0.1 \
--mysql-port=3307 --mysql-user=root --mysql-password=letmein \
tests/include/oltp_legacy/insert.lua prepare

##### 插入数据测试，运行压测准备命令
##### 注意时间 --time 设置，尽可能大，从而完成压测; --threads=10，因为测试机器是10核
sysbench --db-driver=mysql --threads=24 --time=86400 --events=50000 --oltp-tables-count=5 \
--oltp_table_size=10000 --mysql-host=127.0.0.1 --mysql-port=3307 \
--mysql-user=root --mysql-password=letmein \
tests/include/oltp_legacy/insert.lua run > results.log &

##### 混合操作
sysbench  --threads=20 --time=6000 --oltp-tables-count=5 --oltp_table_size=1000 \
--mysql-host=127.0.0.1 --mysql-port=3307 \
--mysql-user=root --mysql-password=letmein \
tests/include/oltp_legacy/oltp.lua run > results.log &

##### 清理测试数据，每次测试之后都需要清理(可以不需要，有清理脚本)
sysbench --db-driver=mysql --mysql-db=sbtest --oltp-tables-count=5 \
--mysql-host=127.0.0.1 --mysql-port=3307 \
--mysql-user=root --mysql-password=letmein \
tests/include/oltp_legacy/oltp.lua cleanup
```

# 五、cryptdb 测试结果

## 5.1 插入测试结果

> 将`wrapper.lua`中的print注释掉

| mysql-proxy配置                 | oltp-tables-count | oltp_table_size | threads | events | Mysql-Proxy CPU(%) | Mysql-Proxy MEM(%) | 时间                          | TPS                | 备注                                                         |
| ------------------------------- | ----------------- | --------------- | ------- | ------ | ------------------ | ------------------ | ----------------------------- | ------------------ | ------------------------------------------------------------ |
| 10核8G内存`--event-threads=4`   | 5                 | 1万             | 10      | 5万    | 21-27              | 90                 | 3259.2550s                    | 15.3               |                                                              |
| 10核8G内存`--event-threads=4`   | 5                 | 1万             | 20      | 万     | 25-35              | 93                 | 3522.0625s                    | 14.19              |                                                              |
| 10核8G内存`--event-threads=4`   | 5                 | 1万             | 20      | 万     | 26-34              | 2.8-79.3           | 2576.6006                     | 19.4               | 将`wrapper.lua`中的`print`注释掉，后面测试都注释掉（还有1条输出没有成功），<font color="red">内存逐渐增加（内存增加不知道与日志有关吗？）</font> |
| 10核8G内存`--event-threads=4`   | 5                 | 1万             | 40      | 万     | 22-33              | 6.4-93.4           | mysql-proxy 卡死了            | ---                | 测试了，40是最高线程数，mysql_proxy 提示`proxy-plugin.c.1865: Cannot connect, all backends are down.`错误 |
| 10核8G内存`--event-threads=4`   | 5                 | 1万             | 40      | 5万    | 20-35              | 2.7-81.7           | 2535.0010                     | <font>19.72</font> | ---                                                          |
| 10核8G内存`--event-threads=10`  | 5                 | 1万             | 20      | 5万    | 36-52              | 1.6-84.1           | 2511.4834                     | 19.91              | CPU还是稳定在35%左右                                         |
| 10核16G内存`--event-threads=10` | 5                 | 1万             | 20      | 5万    | 41-33-24           | 0.8-38.3           | 2424.4091                     | 20.62              | ---                                                          |
| 10核16G内存`--event-threads=10` | 5                 | 1万             | 40      | 5万    | 90+                | 2+                 | 444.4250                      | 112.50             | 将历史数据删除后，新插入数据的时间                           |
| 10核16G内存`--event-threads=10` | 5                 | 10万            | 40      | 50万   | 96                 | 42                 | <font color="red">报错</font> | 报错               | 每个表插入32417数据后报错                                    |

==<font color="red">总结</font>：在所有的测试中，sysbench的CPU和MEM使用率都基本是0。所以`mysql-proxy`是性能瓶颈，需要更改`mysql-proxy`的实现部分。==

## 5.2 混合测试结果

| mysql-proxy配置                 | oltp-tables-count | oltp_table_size | threads | events | CPU(%)                        | MEM(%) | 时间 | TPS  | 备注 |
| ------------------------------- | ----------------- | --------------- | ------- | ------ | ----------------------------- | ------ | ---- | ---- | ---- |
| 10核16G内存`--event-threads=10` | 5                 | 1万             | 20      | 5万    | <font color="red">报错</font> | ---    | ---  | ---  | ---  |
| 10核16G内存`--event-threads=10` | 5                 | 1万             | 40      | 5万    | ---                           | ---    | ---  | ---  | ---  |

==1万数据测试会报错==：

```bash
unexpected packet type 4
unexpected packet type 4
unexpected packet type 4
unexpected packet type 4
unexpected packet type 4
Adjusting onion!
onion: oOrder

ADJUST: 
 UPDATE `sbtest`.table_UVEKEPGUJX    SET JEWVFXQPPGoOrder = cast(cryptdb_decrypt_int_sem(`sbtest`.`table_UVEKEPGUJX`.`JEWVFXQPPGoOrder` AS `JEWVFXQPPGoOrder`,'?$????+xs4LQFD??',`sbtest`.`table_UVEKEPGUJX`.`cdb_saltUCUWFFZKOC` AS `cdb_saltUCUWFFZKOC`) as unsigned);
Adjusting onion!
onion: oOrder
```

==出错之后，删除`shadow`目录都无法正常使用cryptdb。==

# 六、Practical-Cryptdb 测试结果

## 6.1 插入测试结果

> 每次插入测试时，都删库重新进行测试

| <div style="width:30px;">硬件配置</div>  | <div style="width:20px;">表个数</div> | <div style="width:60px;">表数据量</div> | <div style="width:20px;">threads</div> | <div style="width:80px;">events</div> | <div style="width:60px;">每个CPU</br>(%)</div> | <div style="width:50px;">MEM</br>(%)</div> | <div style="width:50px;">时间</div> | <div style="width:50px;">TPS</div> | 备注 |
| ---------------------------------------------------- | ------------------------------------- | --------------------------------------- | -------------------------------------- | ------------------------------------- | ---------------------------------------------- | ----------------------------------------------------- | ----------------------------------------------------- | ------------------------------------- | ------------------------------------------------------------ |
| 10核16G内存 | 5 | 1000 | 20 | 5000 | 58-50 | 0.9-5.1  | 352.4233 | 14.19 | 内存增加CPU减少 |
| 10核16G内存 | 5 | 5000 | 20 | 2.5万 | --- | 23.1</br><font color="red">＜10%才是有效的修改</font> | ---  | ---  | 更改1.5：修改`query_parse`中线程回收代码 |
| 10核16G内存 | 5 | 1万 | 20 | 5万 | 45-77 | 0.7-23.9 | 2990.4383 | 16.72 | 内存增加，CPU反而在减少  |
| 10核16G内存 | 5 | 1万 | 30 | 5万 | 46-60 | 1.4-45.4 | 2977.0807 | 16.79 | <font color="red">更改1：回收部分内存之后1w数据内存仍没有明显减少</font> |
| 10核16G内存 | 5 | 10万 | 20 | 50万 | 58-65 | 100  | ---  | ---  | 更改1：修改内存之后是32359左右内存占满报错退出 |
| <div style="background-color:#ffffcc;">24/5</div> | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | ===== |
| 10核16G内存 | 5 | 1万 | 20 | 5万 | 47.2% | <font color="red"><b>0.4→3.3</b></font> | 2982.6220 | 16.76 | <font color="red"><b>更改2：线程回收机制更改之后，内存仍然在缓慢增加，仍然上百兆</b></font> |
| 10核16G内存 | 5 | 10万 | 20 | 50万 | 33.2  | <font color="red"><b>0.4→11.5</b></font>  | 26112.1243（7.25小时） | 19.15 | <font color="red"><b>更改2：线程回收机制更改之后</b></font>  |
| 16核32G内存 | 5 | 1万 | 20 | 5万 | 72.9 → 89.2 | 0.1 → 0.9 | 1491.9790</br>(24分钟) | <font color="red"><b>33.51</b></font> | <font color="red">高性能虚拟机上测试，CPU增加速度大于内存，同时TPS提升一倍</font> |
| 16核32G内存 | 5 | 10万 | 20 | 50万 | 64.6 → 89.9 | 0.2 → 3.1 | 9207.7369</br>(2.55小时)  | <font color="red"><b>54.30</b></font> | mysql-proxy，TPS又有提升 |
| <div style="background-color:#ffffcc;">24/5</div> | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | ===== |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 94.5  | 0.9  | 3078.3654 | 16.24 | 内存未提升  |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 89.1  | 0.9  | 1488.5990 | 33.59 | 虚拟机重启后相同程序进行测试 |
| 24核64G内存 | 5 | 100万  | 24 | 500万 | 87 | <font color="red"><b>21.7</b></font>  | 83545.1622</br>(23.2小时)  | 59.85 | 内存还是有些高  |
| 24核64G内存 | 5 | 1000万 | 24 | 5千万 | --- | ---  | ---  | ---  | <font color="red">CPU不平衡，6天插入390w+数据，且内存到94%</font> |
| <div style="background-color:#ffffcc;">24/6/7</div>  | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | ===== |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | <font color="red"><b>19</b></font> | 0.9  | 1499.2282 | 33.35 | <font color="red"><b>修改之后，CPU从89.1%降低至19%了</b></font> |
| 24核64G内存 | 5 | 10万 | 24 | 50万 | 87.7  | 3.1  | 8834.6623</br>(2.45小时)  | 56.6 | 稍微快一点  |
| <div style="background-color:#ffffcc;">24/6/12</div> | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | 在`lextoQuery`函数中释放`create_list` |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 86.8  | 0.9  | <font color='red'>1492.4812</font> | <font color='red'>33.5</font> | <font color='red'>对标</font> |
| 24核64G内存 | 5 | 10万 | 24 | 50万 | 87.3  | 3.1  | 9051.493 | 55.24 | 与之前性能差不多 |
| <div style="background-color:#ffffcc;">24/7/2</div>  | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | SQL-INSERT多线程 |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 88.5  | 4.9  | 1628.9267 | 30.69 | 4线程，比之前变慢 |
| <div style="background-color:#ffffcc;">24/7/19</div> | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | NTL升级加Paillier多线程  |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 89.3  | 0.9  | 1262.9374 | 39.59 | 使用<font color="red"><b>NTL 11.5.1</b></font>和<font color="red"><b> GMP 6.3.0</b></font>，多线程安全，自动开启线程增强功能，但是<font color="red"><b>未设置多线程</b></font> |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 88.9  | 0.9  | 1180.5195 | 42.35 | 使用<font color="red"><b>NTL 11.5.1</b></font>和<font color="red"><b> GMP 6.3.0</b></font>，多线程安全，自动开启线程增强功能，<font color="red"><b>`HOM::encrypt()` 多线程后台生成大质数</b></font> |
| 24核64G内存 | 5 | 1000万 | 24 | 5千万 | 92 | 60.8 | ---  | 60 | 经过195889秒能压测大概11,833,710，TPS在60左右 |
| <div style="background-color:#ffffcc;">24/7/22</div> | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | 使用NTL的BasicThreadPool功能 |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 88.8  | 0.9  | 1189.9213 | 42.02 | 都设置为`1`线程，等待结果 |
| 24核64G内存 | 5 | 100万  | 24 | 500万 | 85.6  | 21.7 | <font color='red'>70274.7410</br>（19.52小时）</font> | <font color='red'>71.15</font> | <font color='red'>比之前快了3.68小时，TPS提高11.3</font> |
| <div style="background-color:#ffcccc;">24/8/8</div>  | = | ==== | =  | ===== | ====== | =======  | =======  | ==== | 使用[PLZ13]的OPE加密，对应代码是`online_ope.hh`和`online_ope.cc` |
| 24核64G内存 | 5 | 1万 | 24 | 5万 | 70.4 | 0.2 | 447.8031 | 111.66 | 等待结果 |
| 24核64G内存 | 5 | 100万  | 24 | 500万 | -- | -- | -- | -- | 等待结果 |
| 24核64G内存 | 5 | 1000万 | 24 | 5000万 | -- | -- | -- | -- | 等待结果 |

==总结说明：==

1.  插入2w数据左右内存达到99%，但达到99%后，后面就不继续增加，而是保持在99%左右，数据仍然继续在插入。
2.  TPS 与 CPU 的配置有关。CPU 配置越高，TPS 越高。但是达不到非 proxy 环境下的 TPS。（==需要测试非proxy环境。==）
3.  如果能做到单表1000w记录，应该时非常不错的性能了。参考《[单台mysql承载量\_MySQL到底能支持多大的数据量](https://blog.csdn.net/weixin_39783156/article/details/113293641)》

## 6.2 混合测试结果

> 出错之后很难恢复

| mysql-proxy配置                 | oltp-tables-count | oltp_table_size | threads | events | CPU(%)                        | MEM(%) | 时间 | TPS  | 备注 |
| ------------------------------- | ----------------- | --------------- | ------- | ------ | ----------------------------- | ------ | ---- | ---- | ---- |
| 10核16G内存`--event-threads=10` | 5                 | 1万             | 20      | 50000  | <font color="red">报错</font> | ---    | ---  | ---  | ---  |
| 10核16G内存`--event-threads=10` | 5                 | 1万             | 40      | 50000  | ---                           | ---    | ---  | ---  | ---  |