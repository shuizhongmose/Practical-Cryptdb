#include "big_proxy.hh"
#include <vector>
#include <iostream>
#include <fstream>
#include <string>

#include<util/util.hh>

using std::string;
using std::vector;

vector<string> queries{
    "show databases;",
    "create database if not exists tdb2;",
    "use tdb2;",
    "create table if not exists student(id integer, name varchar(50),age integer) ENGINE=InnoDB;",
    "INSERT INTO student VALUES (1, 'shao', 20);",
    "INSERT INTO student VALUES (2, 'xiaocai', 21);",
    "INSERT INTO student VALUES (3, 'nans', 22);",
    "INSERT INTO student VALUES (4, 'hehe', 23);",
    "INSERT INTO student VALUES (5, 'oo', 24);",
    "INSERT INTO student VALUES (6, 'lihua', 19);",
    "INSERT INTO student VALUES (7, 'wangwu', 20);",
    "INSERT INTO student VALUES (8, 'zhaoliu', 21);",
    "INSERT INTO student VALUES (9, 'liming', 22);",
    "INSERT INTO student VALUES (10, 'zhangsan', 23);"
    // "INSERT INTO student VALUES (1, 'shao', 20), (2, 'xiaocai', 21), (3, 'nans', 22), (4, 'hehe', 23), (5, 'oo', 24);",
    // "INSERT INTO student VALUES (6, 'lihua', 19), (7, 'wangwu', 20), (8, 'zhaoliu', 21), (9, 'liming', 22), (10, 'zhangsan', 23);",
    // "INSERT INTO student VALUES (11, 'lisi', 20), (12, 'xiaoming', 24), (13, 'xiaohong', 23), (14, 'dazhuang', 22), (15, 'xiaobai', 21);",
    // "INSERT INTO student VALUES (16, 'dongdong', 22), (17, 'yaya', 23), (18, 'tianya', 24), (19, 'haihai', 25), (20, 'lianhua', 20);",
    // "INSERT INTO student VALUES (21, 'xiaoxue', 21), (22, 'yingying', 22), (23, 'xiaoli', 23), (24, 'junjun', 24), (25, 'dongmei', 25);",
    // "INSERT INTO student VALUES (26, 'zhuzi', 20), (27, 'xuanxuan', 21), (28, 'jinjin', 22), (29, 'yoyo', 23), (30, 'mumu', 24);",
    // "INSERT INTO student VALUES (31, 'guoguo', 19), (32, 'xiaoyi', 20), (33, 'huahua', 21), (34, 'niuniu', 22), (35, 'miaomiao', 23);",
    // "INSERT INTO student VALUES (36, 'hanhan', 24), (37, 'yuanyuan', 25), (38, 'huizi', 20), (39, 'lanlan', 21), (40, 'bingbing', 22);",
    // "INSERT INTO student VALUES (41, 'honghong', 23), (42, 'qiuqiu', 24), (43, 'longlong', 25), (44, 'linlin', 20), (45, 'pingping', 21);",
    // "INSERT INTO student VALUES (46, 'xinxin', 22), (47, 'meimei', 23), (48, 'xiaobing', 24), (49, 'xiaoxin', 25), (50, 'doudou', 19);"
//    "select * from student;",
//    "select id from student;",
//    "select name from student;",
//    "select sum(id) from student;"
};


vector<string> queries1{
    "show databases;",
    "create database if not exists tdb2;",
    "use tdb2;",
    "CREATE TABLE IF NOT EXISTS student ( id INTEGER PRIMARY KEY, col1 CHAR(10), col2 CHAR(10), col3 CHAR(10), col4 CHAR(10) ) ENGINE=InnoDB;",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (1, 'abcdefghij', 'efghijklmn', 'ijklmnopqr', 'mnopqrstuv');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (2, 'qrstuvwxy', 'uvwxyzabcd', 'yzabcdefghijkl', 'cdefghijkl');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (3, 'ghijklmnop', 'klmnopqrst', 'opqrstuvwx', 'stuvwxyzab');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (4, 'wxyzabcdef', 'abcdefghij', 'efghijklmn', 'ijklmnopqr');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (5, 'mnopqrstuv', 'qrstuvwxyz', 'uvwxyzabcd', 'yzabcdefghijkl');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (6, 'cdefghijkl', 'ghijklmnop', 'klmnopqrst', 'opqrstuvwx');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (7, 'stuvwxyzab', 'wxyzabcdefgh', 'abcdefghij', 'efghijklmn');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (8, 'ijklmnopqr', 'mnopqrstuv', 'qrstuvwxyz', 'uvwxyzabcd');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (9, 'yzabcdefghijkl', 'cdefghijkl', 'ghijklmnop', 'klmnopqrst');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (10, 'opqrstuvwx', 'stuvwxyzab', 'wxyzabcdef', 'abcdefghij');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (10, 'opqrstuvwx', 'stuvwxyzab', 'wxyzabcdef', 'abcdefghij');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (11, 'klmnopqrst', 'mnopqrstuv', 'qrstuvwxyz', 'ijklmnopqr');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (12, 'uvwxyzabcd', 'yzabcdefgh', 'cdefghijkl', 'mnopqrstuv');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (13, 'ghijklmnop', 'ijklmnopqr', 'mnopqrstuv', 'rstuvwxyzab');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (14, 'bcdefghijk', 'fghijklmnop', 'stuvwxyzabc', 'defghijklm');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (15, 'cdefghijkl', 'yzabcdmnop', 'rstuvwxyzab', 'mnopqrstuv');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (16, 'ijklmnopqr', 'qrstuvwxy', 'abcdefhijk', 'uvwxyzabcd');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (17, 'fghijklmnop', 'lmstuwxyz', 'vwxyzabc', 'lmopqrstuv');",
    "INSERT INTO student (id, col1, col2, col3, col4) VALUES (18, 'mnopqrstu', 'tuvwxyzabc', 'abcdefghijk', 'wxyzabcdefhij');"
};

int
main(int argc,char ** argv) {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){  
        perror("getcwd error");  
    }
    string embeddedDir = std::string(buffer)+"/shadow";
    free(buffer);
    init_mysql(embeddedDir);

    {
        big_proxy b;
        
        // // 插入10000行数据
        // std::ostringstream oss;
        // for (int i=1; i<=100; i+=5) {
        //     oss.str("");
        //     oss << "INSERT INTO student VALUES ("
        //         << i << ", 'shao" << i << "', 20), "
        //         << "(" << i+1 << ", 'xiaocai" << i << "', 21), "
        //         << "(" << i+2 << ", 'nans" << i << "', 22), "
        //         << "(" << i+3 << ", 'hehe" << i << "', 23), "
        //         << "(" << i+4 << ", 'oo" << i << "', 24);";
        //     queries.push_back(oss.str());
        // }

        for(auto query:queries){
            LOG(debug) <<"============================ beging ============================";
            size_t before = getCurrentRSS();
            b.go(query);
            size_t after = getCurrentRSS();
            LOG(debug) <<query<<"   pass!";
            LOG(debug) << "Total memory: " << after << " bytes, Memory usage change: " << (after - before) << " bytes";
            LOG(debug) <<"============================ end ============================";
        }
        LOG(debug) <<"pass ALL !!"<<std::endl;
    }

    clear_embeddedmysql();
    return 0;
}
