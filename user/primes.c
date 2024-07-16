#include "kernel/types.h"
#include "user/user.h"

#define MAXNUM 36

void 
prime(int pipeLeft[])
{
    int primeNum;
    close(pipeLeft[1]);//关闭写端

    if(read(pipeLeft[0],&primeNum,4)<=0){
        close(pipeLeft[0]);
        exit(0);
    }
    printf("prime %d\n", primeNum);

    int pipeRight[2];
    pipe(pipeRight);

    int pid = fork();
    if(pid==0){
        close(pipeRight[1]);//关闭子进程不需要的写端
        prime(pipeRight);
    }
    // 当前进程：父进程
    else if(pid > 0){
        close(pipeRight[0]);//关闭父进程不需要的读端

        int num;
        while(read(pipeLeft[0],&num,4) > 0){
            if(num % primeNum != 0){
                write(pipeRight[1],&num,4);
            }
        }

        close(pipeLeft[0]);//关闭读端
        close(pipeRight[1]);//关闭写端
        wait(0);
    }
    else{
        printf("ERROR:Create Child Failed.");
    }

    exit(0);
}

int 
main(int argc, char *argv[])
{
    int pipeRight[2];
    pipe(pipeRight);

    for(int i = 2; i < MAXNUM; i++){
        write(pipeRight[1],&i,4);
    }

    close(pipeRight[1]);//关闭写端
    prime(pipeRight);

    exit(0);
}