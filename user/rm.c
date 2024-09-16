#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){ // 检查命令行参数的数量是否少于2。在UNIX命令中，argv[0] 是程序名，所以至少需要有一个参数（即要删除的文件）
    fprintf(2, "Usage: rm files...\n"); // 如果参数不足，向标准错误（文件描述符2）输出使用方法，并提示用户如何正确使用该命令。
    exit(1);
  }

  for(i = 1; i < argc; i++){ // 从argv[1]开始遍历所有参数，这些参数是用户希望删除的文件名
    if(unlink(argv[i]) < 0){ //尝试删除指定的文件。unlink函数是一个系统调用，用于删除一个文件的目录项和减少其链接数。如果unlink返回值小于0，表示删除失败。
      fprintf(2, "rm: %s failed to delete\n", argv[i]); // 如果删除失败，向标准错误输出错误信息，指明哪个文件删除失败
      break;
    }
  }

  exit(0);
}
