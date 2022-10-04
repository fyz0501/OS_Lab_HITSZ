#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAX 1024

void main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(2, "usage: find command\n"); // 异常退出 exit(1)
         exit(1);
    }
    char buf[MAX];                        //buf用于存储读入数据
    //char* origin = buf;                 //记录起始地址
    char* pointer = buf;
    // int ret;
    char* args[MAX];
    for (int i = 1; i < argc; i++) {
        args[i-1] = argv[i];            // 将当前参数的地址赋给args
    }
    int n = 0;
    char* start = buf;
    while (read(0, pointer, 1) > 0) {   // 逐字节读入数据到指针所指位置中
        n++;
        if (*pointer == '\n') {            // 读完一行数据
            *pointer = 0;                  // 将换行符换作0，
            if (fork() == 0) {
                args[argc-1] = start;     // 从起始位置开始，将一行数据作为一个参数传进去
                exec(args[0], args);
                exit(0);
            } else {
                pointer = buf+n; 
                start = buf+n;            //下一次作为一个参数的一行数据的起始位置，更改为buf+n
                n = 0;
                wait(0);                  //等待子进程执行结束
            }
        } 
        else {
           pointer++;
        }
    }
    exit(0);
}
