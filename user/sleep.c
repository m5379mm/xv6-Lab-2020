#include "kernel/types.h"
#include "user/user.h"

// argc:命令行参数的数量
// argv：指向字符串数组的指针，每个字符串都是一个命令行参数
int 
main(int argc,char *argv[])
{
    // 未传参情况
    if(argc<2){
        printf("ERROR: Need time parameter.\n");
    }
    else{
        sleep(atoi(argv[1]));
    }
    exit(0);
}