#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAX 1024

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(2, "usage: find command\n"); // 异常退出 exit(1)
         exit(1);
    }
    // 有若干参数，将参数存到新的变长数组
    char* args[MAX]  = {'\0'};
    for (int i = 1; i < argc; ++i) {
        args[i-1] = argv[i];            // 传给子进程的参数
    }
    char buf[MAX];
    //char* origin = buf;                 //记录起始位置
    char* pointer = buf;
    int ret;
    while ((ret = read(0, pointer, 1)) > 0) {   // 读入数据到指针所指位置中，即buf处
        if (*pointer != '\n') {               // *输入为换行符，创立子进程
            pointer++;
        } else {
            *pointer = '\0';                     // *将换行符换作\0，表示读完了一个
            if (fork() == 0) {          // 创建子进程来执行子命令
                args[argc-1] = buf;     // 将输入的一行当作一个参数
                exec(args[0], args);
                exit(0);
            } else {
                wait(0);                // 等待子进程退出
            }
            pointer = buf;                    // 重新指向开头
        }
    }
    exit(0);
}
