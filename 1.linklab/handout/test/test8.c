#include <stdio.h>
#include <stdlib.h>

int main() {
    void* a, *b;
    a = realloc(NULL, 20); // 얘도 log는 realloc으로 나와야하는데 아예 malloc으로 나옴.
    b = malloc(0);
    free(NULL); // nothing happens. 근데 log는 나와야지

    a = malloc(5);
    b = realloc(a, 0); // 직전 malloc을 free시켜야 함.
    b = realloc((void*)0xdeadbeef, 0); // ill free
    b = realloc(a, 0); // double free

    b = calloc(0, 20); // null return 하거나 걍 ptr return이니까 원래 calloc함수가 하는대로 놔둠.
    b = calloc(4, 0); // 상동
}
