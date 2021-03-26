#include <stdio.h>
#include <stdlib.h>

int main() {
    // 1. malloc size = 0인 경우 -> either null or a unique pointer
    //      그냥 null 출력하게 만듦.
    void* a = malloc(0);

    // 2. calloc ptr, size 둘 중 최소 하나 0인 경우.
    void* b = calloc(0, 20);    // null return하게 만듦.
    b = calloc(4, 0);           // 상동.
    b = calloc(0, 0);           // 상동.

    // 3. realloc ptr null인 경우 -> malloc(size)역할.
    void* c = realloc(NULL, 20);
    c = realloc(NULL, 0);

    // 4. realloc size == 0인 경우. free() 역할 함.
    void* d = malloc(5);
    void* e = realloc(d, 0);            // 직전 malloc을 free시켜야 함.
    e = realloc((void*)0xdeadbeef, 0);  // ill free
    e = realloc(d, 0);                  // double free

}
// line 15, 19에서만 실제로 allocated되므로 total / 2하면 됨.

