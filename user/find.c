#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
//在类Unix文件系统中，每个目录都包含两个特殊的目录条目：. 和 ..
//这些条目在每个目录中都自动存在，不需要用户显式创建。
//为了避免递归使不进入父目录，递归时需要比较目录下目录名是否为.或..

// 获取路径中的文件名部分,用于判断是否为所查找文件
char* getName(char* path)
{
    char *p;
    // 从后向前查找，到第一个/停止
    for(p = path + strlen(path); p >= path && *p != '/'; p--);
    p++;
    return p;
}

void find(char *path, char *name)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // 打开路径
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取路径的文件信息
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 处理文件和目录
    switch (st.type) {
    case T_FILE:
        // 如果是文件，比较文件名
        if (strcmp(getName(path), name) == 0) {
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            break;
        }

        // 将当前路径拷贝到缓冲区
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        // 遍历目录条目
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            //inum为0表示目录项无效；确定目录名称不为.或..
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            //更新路径
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }

            // 递归调用 find 函数
            find(buf, name);
        }
        break;
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    // 检查传入参数
    if (argc != 3) {
        printf("ERROR: find <path> <name>\n");
        exit(0);
    }

    // 调用 find 函数
    find(argv[1], argv[2]);
    exit(0);
}
