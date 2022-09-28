#include "kernel/types.h"
#include "user.h"


void main(int argc,char* argv[]){
    int p[35][2];
    int n = 2;  //起始质数为2
    int primes[34] = {0};
    int len_primes = sizeof(primes)/4-1;
    for(int i = 0; i < len_primes; i++){
        primes[i] = i+2;
    }
    int num_temp;
    while(n!=0){
       // printf("2此时的n为：%d\n",n);
        pipe(p[n]);
        int pid = fork();
        if(pid > 0){//父进程写数据 
            close(p[n][0]);//关闭左边管道的读端口
            for(int i = 0;i<=len_primes;i++){
                if(primes[i]%n !=0&&primes[i]!=0 ){
                    num_temp = primes[i];
                    write(p[n][1],(char *)&num_temp,1);
                }
            }
            if(n!=0){
                printf("prime %d\n",n);
            }
            close(p[n][1]);//写入完成，关闭父进程管道的写端口
            wait(0);
            exit(0);
        }
        else if(pid == 0){
            close(p[n][1]);
            int last_n = n;
            int i = 0;
            if(read(p[n][0],(char *)&num_temp,1)>0){
                primes[i] = num_temp;
                i++;
                while(read(p[n][0],(char *)&num_temp,1)>0){
                    primes[i] = num_temp;
                    i++;
                }
                    len_primes = i-1;//更新最新的primes数组的长度
                    n = primes[0];//令n等于下一轮需要输出的质数
            }
            else{
                n = 0;
            }
            close(p[last_n][0]);//关闭子进程管道的读端口
        }
    }
    exit(0);
}