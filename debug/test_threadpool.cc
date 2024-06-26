#include <main/thread_pool.hh> // 确保ThreadPool类的头文件路径正确

int
main() { 
    ThreadPool pool(4);
    return true;
}