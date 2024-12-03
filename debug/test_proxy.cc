#include "main/big_proxy.hh"
#include <vector>
using std::string;
using std::vector;
int
main(int argc,char ** argv) {
    big_proxy b;
    vector<string>queries={
        "show databases;",
        "CREATE DATABASE `sbtest` DEFAULT CHARACTER SET utf8;",
        "use sbtest;",
        "CREATE TABLE user ( id INT AUTO_INCREMENT PRIMARY KEY, name TEXT, age INTEGER) ENGINE=InnoDB;",
        "INSERT INTO user (name, age) VALUES ('alice', 19), ('bob', 20), ('chris', 21);",
        "INSERT INTO user (name, age) VALUES ('David', 19), ('Ely', 20), ('Flower', 21);",
        "INSERT INTO user (name, age) VALUES ('George', 19), ('Henry', 20), ('Ivan', 21);",
        "select * from user;"
    };
    std::string query;
    try {
        for(auto query:queries)
            b.go(query);
    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception caught!" << std::endl;
        return false;
    }
    return 0;
}

