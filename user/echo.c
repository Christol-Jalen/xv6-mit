#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
argc：参数的数量
argv：参数的内容

这个程序的核心功能是模拟echo命令的基本行为，即打印命令行提供的所有参数，每个参数之间用一个空格分隔，最后以换行符结束输出。
*/

int
main(int argc, char *argv[]) 
{
  int i;

  for(i = 1; i < argc; i++){ // 对每一个参数进行操作，默认跳过第一个参数argv[0]（通常为程序的名称）
    write(1, argv[i], strlen(argv[i])); // 将参数写入文件描述符1（标准输出）
    if(i + 1 < argc){
      write(1, " ", 1); // 在参数间添加空格
    } else {
      write(1, "\n", 1); // 在最后一个参数后换行
    }
  }
  exit(0); // 退出程序
}
