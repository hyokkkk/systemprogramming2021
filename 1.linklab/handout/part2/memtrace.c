//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
    // ...

    // to avoid divide by 0
    unsigned long alloc_avg = n_malloc+n_calloc+n_realloc == 0 ?
                              0 : n_allocb/(n_malloc+n_calloc+n_realloc);

    LOG_STATISTICS(n_allocb, alloc_avg, n_freeb);

    // ptr, size, ref cnt
    //TODO: nonfreed가 0byte면 이거 인쇄 안 한다.
    if (n_allocb != n_freeb){
        LOG_NONFREED_START();
    //TODO: 아마도 linked list 없어질 때까지 for문 돌려아 할듯?
        LOG_BLOCK(0L, 0L, 0L);
    }
    //TODO: delete

    LOG_STOP();

    // free list (not needed for part 1)
    free_list(list);
}

void* malloc(size_t size){
    char* error;
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    // Get address of libc malloc
    mallocp = dlsym(RTLD_NEXT, "malloc");

    // call libc malloc
    void* ptr = mallocp(size);

    alloc(list, ptr, size);     // insert in the list
    n_allocb += size;           // total allocated bytes
    n_malloc ++;                // total malloc call
    // printf invokes segfault.
    mlog("          malloc( %ld ) = %p", size, ptr);
    return ptr;
}

void* calloc(size_t nmemb, size_t size){
    char* error;
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    // Get address of libc calloc
    callocp = dlsym(RTLD_NEXT, "calloc");

    // call libc calloc
    void* ptr = callocp(nmemb, size);

    unsigned long csize = nmemb * size;
    alloc(list, ptr, csize);    // insert in the list
    n_allocb += csize;          // total allocated bytes
    n_calloc ++;                // total calloc call
    mlog("          calloc( %ld, %ld ) = %p", nmemb, size, ptr);
    return ptr;
}

void* realloc(void* rptr, size_t size){
    char* error;
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    // Get address of libc realloc
    reallocp = dlsym(RTLD_NEXT, "realloc");

    // call libc realloc
    void* ptr = reallocp(rptr, size);

    n_freeb += dealloc(list, rptr)->size;
    alloc(list, rptr, size);
    n_allocb += size;   // total allocated bytes
    n_realloc ++;       // total realloc call
    mlog("          realloc( %p, %ld ) = %p", rptr, size, ptr);
    return ptr;
}

void free(void* ptr){
    char* error;
    if (!ptr){ return; }
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    freep = dlsym(RTLD_NEXT, "free");       // Get address of libc free
    n_freeb += dealloc(list, ptr)->size;    //freed bytes
    freep(ptr);     // call libc realloc
    mlog("          free( %p )", ptr);
}
