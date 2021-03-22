// test5 revised
#include <stdlib.h>


int main(void)
{
  void *a;

  a = malloc(10);
  a = realloc(a, 100);
  a = realloc(a, 1000);
  a = realloc(a, 10000);
  a = realloc(a, 100000);
  free(a);

  void *b; // 사이즈가 작아져서 아무 일도 안 일어날 때
  b = malloc(30);
  b = realloc(b, 20);
  b = realloc(b, 50);


  return 0;
}
