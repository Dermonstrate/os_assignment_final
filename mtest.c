#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <alloca.h>
#include <stdlib.h>

int bss_var;
int data_var = 93;

void function(void) {
static int level = 0;
auto int stack_var;

if (++level == 3)
return;

printf("\t栈级 %d: stack_var地址: %p\n",
level, & stack_var);
function();
}

int main()
{
char *p, *b, *nb;

printf("代码段地址：\n");
printf("\t主函数代码段： %p\n", main);
printf("\tfunction代码段 %p\n", function);

printf("栈地址：\n");
function();

p = (char *) alloca(32);
if (p != NULL) {
printf("\t alloca()开始: %p\n", p);
printf("\t alloca()结束: %p\n", p + 31);
}

printf("数据段地址：\n");
printf("\tdata_var的地址: %p\n", & data_var);

printf("BSS地址:\n");
printf("\tbss_var的地址: %p\n", & bss_var);

b = sbrk((ptrdiff_t) 32);
nb = sbrk((ptrdiff_t) 0);
printf("堆地址:\n");
printf("\t开始时堆末地址: %p\n", b);
printf("\t新堆末地址: %p\n", nb);

b = sbrk((ptrdiff_t) -16);
nb = sbrk((ptrdiff_t) 0);
printf("\t结束时堆末地址: %p\n", nb);
}

