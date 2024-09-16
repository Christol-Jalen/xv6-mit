#include "kernel/types.h"
#include "user/user.h"

#define RD 0 //pipe的read端
#define WR 1 //pipe的write端

int main(int argc, char const *argv[]) {
    char buf = 'b'; //用于传送的字节

    // 创建两个整数数组，用于存储管道文件描述符
    int fd_c2p[2]; //子进程->父进程
    int fd_p2c[2]; //父进程->子进程

    // 创建两个管道
    pipe(fd_c2p);
    pipe(fd_p2c);

    int pid = fork();
    int exit_status = 0;

    if (pid < 0) {
        fprintf(2, "fork() error!\n");
        close(fd_c2p[RD]);
        close(fd_c2p[WR]);
        close(fd_p2c[RD]);
        close(fd_p2c[WR]);
        exit(1);
    } else if (pid == 0) { //子进程，通过关闭不需要的管道端口（写端fd_p2c[WR]和读端fd_c2p[RD]）进行通信设置
        close(fd_p2c[WR]);
        close(fd_c2p[RD]);

        if (read(fd_p2c[RD], &buf, sizeof(char)) != sizeof(char)) { // 子进程首先尝试从父进程读取数据（通过fd_p2c[RD]）
            fprintf(2, "child read() error!\n");
            exit_status = 1; //标记出错
        } else {
            fprintf(1, "%d: received ping\n", getpid()); // 如果读取成功，打印接收到的”ping”消息
        }

        if (write(fd_c2p[WR], &buf, sizeof(char)) != sizeof(char)) { // 然后尝试将相同的数据写回给父进程（通过fd_c2p[WR]）
            fprintf(2, "child write() error!\n");
            exit_status = 1;
        }
        // 关闭剩余的管道端口并退出子进程
        close(fd_p2c[RD]);
        close(fd_c2p[WR]);
        exit(exit_status);
    } else { //父进程，同样通过关闭不需要的管道端口（读端fd_p2c[RD]和写端fd_c2p[WR]）设置通信
        close(fd_p2c[RD]);
        close(fd_c2p[WR]);

        if (write(fd_p2c[WR], &buf, sizeof(char)) != sizeof(char)) { // 父进程首先向子进程发送数据（通过fd_p2c[WR]）
            fprintf(2, "parent write() error!\n");
            exit_status = 1;
        }

        if (read(fd_c2p[RD], &buf, sizeof(char)) != sizeof(char)) { // 然后尝试从子进程读取数据（通过fd_c2p[RD]）
            fprintf(2, "parent read() error!\n"); 
            exit_status = 1; //标记出错
        } else {
            fprintf(1, "%d: received pong\n", getpid()); // 如果读取成功，打印接收到的”pong”消息
        }
        // 关闭剩余的管道端口并根据需要退出父进程
        close(fd_p2c[WR]);
        close(fd_c2p[RD]);
        exit(exit_status);
    }
}