// test4 revised
#include <stdlib.h>

int main(void)
{
  void *a;

  a = malloc(1024);
  free(a);
  free(a); // double free
  free((void*)0xcafe); // ill free
  realloc(a, 20); // double free
  void* b = malloc(40);
  realloc(b, 30); // size가 줄어들어서 nothing happens
  realloc((void*)0xdeadbeef, 50); // ill free

  /*statistics*/
  // total alloc : 1024 + 20 + 40 + 50 = 1134
  // alloc avg : 1134 / 4 (line7 + line11 + line12 + line14 = 4번)
  // freed bytes : line8에서 1024

  return 0;
}
