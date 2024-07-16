#include "kernel/types.h"
#include "user/user.h"
#define N 5
char buffer[N];//定义传递数组
void 
ping(int *parentToChild,int *childToParent)
{
    if(write(parentToChild[1],"ping",N-1)!=N-1){
        printf("ERROR:Write Failed.");
    }
    if(read(childToParent[0],buffer,N)<0){
        printf("ERROR:Read Failed.");
    }
    printf("%d: received %s\n", getpid(), buffer);
}

void
pong(int *parentToChild,int *childToParent)
{
    if(read(parentToChild[0],buffer,N)<0){
        printf("ERROR:Read Failed.");
    }
    printf("%d: received %s\n", getpid(), buffer);
    if(write(childToParent[1],"pong",N-1)!=N-1){
        printf("ERROR:Write Failed.");
    }
}

int 
main(int argc,char *argv[])
{
    int parentToChild[2],childToParent[2];//定义管道
    int pid;//进程号

    // 创建管道:pipe通常以一个包含两个元素的整数数组p为数组，在执行完pipe命令后，
    // p[0]为从管道中读入数据的文件描述符，p[1]为从管道中读出数据的文件描述符
    // 安全判断
    if(pipe(parentToChild)<0||pipe(childToParent)<0){
        printf("ERROR:Create Failed.");
    }
    // 创建进程
    pid = fork();
    if(pid<0){
        printf("ERROR:Create Child Failed.");
    }
    else{
        if(pid==0){
            pong(parentToChild,childToParent);
        }
        else{
            ping(parentToChild,childToParent);
        }
    }
    exit(0);
}
