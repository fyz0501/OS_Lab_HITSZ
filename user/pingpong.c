#include "kernel/types.h"
#include "user.h"


int main(int argc,char* argv[]){
    int p1[2],p2[2];
   int ret1,ret2;
    ret1 = pipe(p1);
    ret2 = pipe(p2);
    char temp[5];
	if(ret1<0||ret2<0)
	{
		printf("error");
		return -1;
	}

	//创建子进程
	int pid = fork();
	if(pid > 0)//父进程写数据 
	{
		close(p1[0]);//关闭管道1的读端口
        close(p2[1]);//关闭管道2的写端口
        write(p1[1],"ping",strlen("ping"));
        close(p1[1]);//写入完成，关闭管道1的写端口
        if (read(p2[0],temp,5)>0){
            int father_pid = getpid();
            printf("%d: received %s\n",father_pid,temp);
        }
        close(p2[0]);//读取完成，关闭管道2的读端口
        
	}
	else if(pid == 0)
	{
		close(p1[1]);//关闭管道1写端口
        close(p2[0]);//关闭管道2读端口
		if(read(p1[0],temp,5)>0){
            int child_pid = getpid();
            printf("%d: received %s\n",child_pid,temp);
        }
        close(p1[0]);//读取完成，关闭管道1读端口d
        write(p2[1],"pong",strlen("pong"));
        close(p2[1]);//写入完成，关闭管道2的写端口

	}
	exit(0);
}