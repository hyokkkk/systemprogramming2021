#include <stdlib.h>

int main(void)
{
  void *a;

  a = malloc(1024);
  free(a);
  free(a);
  free((void*)0x1706e90);
  realloc(a, 20);
  void* b = malloc(40);
  realloc(b, 20);
  realloc((void*)0x1706e98, 20);

  return 0;
}
