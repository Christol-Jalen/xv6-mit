// Simple grep.  Only supports ^ . * $ operators.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[1024]; // 定义全局缓冲区
int match(char*, char*);

/*
char *pattern：用户指定的模式字符串
int fd：文件描述符，指定从哪个文件读取数据

grep (global regular expression print)
这段代码实现了grep功能的核心逻辑，实现了文本搜索功能，用于在文件中搜索包含指定模式的字符串。
*/

void
grep(char *pattern, int fd)
{
  int n, m; // n用于存储每次read调用读取的字节数，m用于累计已读取的字节数
  char *p, *q; // 指针，p用于遍历缓冲区中的数据，q用于查找行结束符（\n）

  m = 0;
  while((n = read(fd, buf+m, sizeof(buf)-m-1)) > 0){ // 循环读取数据到缓冲区，直到没有更多数据
    m += n;
    buf[m] = '\0'; // 将缓冲区末尾设置为\0（空字符），以确保字符串操作安全
    p = buf;
    while((q = strchr(p, '\n')) != 0){ // 使用strchr寻找换行符，分割出单独的行
      *q = 0;
      if(match(pattern, p)){ // 对每行使用match函数检查是否与模式匹配
        *q = '\n';
        write(1, p, q+1 - p); // 如果匹配，将该行输出到标准输出
      }
      p = q+1;
    }
    if(m > 0){
      m -= p - buf;
      memmove(buf, p, m); // 处理缓冲区中未处理的数据，将其移动到缓冲区开始处
    }
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;
  char *pattern; // 要搜索的模式

  if(argc <= 1){ // 检查参数数量，确保至少提供了一个模式
    fprintf(2, "usage: grep pattern [file ...]\n");
    exit(1);
  }
  pattern = argv[1];

  if(argc <= 2){ // 如果只提供了模式，从标准输入读取数据
    grep(pattern, 0);
    exit(0);
  }

  // 如果还指定了文件名，尝试打开文件读取数据
  for(i = 2; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf("grep: cannot open %s\n", argv[i]);
      exit(1);
    }
    grep(pattern, fd); // 调用grep函数进行搜索
    close(fd);
  }
  exit(0);
}

// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

