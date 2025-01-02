#include "big_proxy.hh"
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <malloc.h>

#include<util/util.hh>

using std::string;
using std::vector;

vector<string> queries{
    "show databases;",
    "drop database if exists sbtest;",
    "CREATE DATABASE `sbtest` DEFAULT CHARACTER SET utf8;",
    "use sbtest;",
    "create table if not exists user (id INT AUTO_INCREMENT PRIMARY KEY,  name TEXT, age INTEGER) ENGINE=InnoDB;",
    "INSERT INTO user (name, age) VALUES ('alice', 19);",
    "INSERT INTO user (name, age) VALUES ('bob', 20);",
    "INSERT INTO user (name, age) VALUES ('chris', 21);",
    "INSERT INTO user (name, age) VALUES ('David', 19);",
    "INSERT INTO user (name, age) VALUES ('Ely', 20);",
    "INSERT INTO user (name, age) VALUES ('Flower', 21);",
    "INSERT INTO user (name, age) VALUES ('George', 19);",
    "INSERT INTO user (name, age) VALUES ('Henry', 20);",
    "INSERT INTO user (name, age) VALUES ('Ivan', 21);",
    "select * from user;",
    "select sum(greatest(age,20)) from user;",
    "select * from user order by age desc;",
    "select * from user where name = 'alice'; "
};

int
main(int argc,char ** argv) {
    std::time_t start_time = std::time(nullptr);

    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){  
        perror("getcwd error");  
    }
    string embeddedDir = std::string(buffer)+"/shadow";
    free(buffer);
    init_mysql(embeddedDir);

    {
        big_proxy b;
        
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
    Rewriter::cleanup();
    size_t after = getCurrentRSS();
    LOG(debug) << "malloc_trim(0) = " << malloc_trim(0);
    LOG(debug) << "Total memory at end of program: " << after;

    std::time_t end_time = std::time(nullptr);
    LOG(debug) << "Total time: " << (end_time - start_time) ;
    return 0;
}
