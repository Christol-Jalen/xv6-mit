#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char const *argv[])
{
  if (argc != 2) { //参数错误
    fprintf(2, "usage: sleep <time>\n"); // 提醒用户正确输入参数
    exit(1);
  }
  sleep(atoi(argv[1])); // 调用atoi函数将字符串参数（argv[1]）转换为整数，表示休眠的秒数
  exit(0);
}