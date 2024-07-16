#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLINE 512

void readInput(char* buf)
{
    // 逐字符读取输入:例如：echo作为标准输出工具，其输出内容显示到终端屏幕上
    char ch;
    int i = 0;
    while (read(0, &ch, 1) == 1 && ch != '\n')
    {
        buf[i++] = ch;
        if (i >= MAXLINE - 1)
        {
            break;
        }
    }
    buf[i] = '\0'; // 修改为字符串
}

int main(int argc, char *argv[])
{
    char buf[MAXLINE];
    char *dealArgv[MAXARG];

    dealArgv[0] = argv[1]; // 要执行的命令
    for (int num=2; num < argc; num++){ // 复制原命令
        dealArgv[num - 1] = argv[num];
    }

    while (1)
    {
        readInput(buf);
        // 读到最后
        if (strlen(buf) == 0) {
            break;
        }
        // 加参数
        dealArgv[argc - 1] = buf;
        
        // 创建进程，调用函数
        int pid = fork();
        if (pid == 0)
        {
            exec(dealArgv[0], dealArgv);
        }
        else if (pid > 0)
        {
            wait(0);
        }
        else
        {
            printf("ERROR:Create Failed.");
        }
    }
    exit(0);
}