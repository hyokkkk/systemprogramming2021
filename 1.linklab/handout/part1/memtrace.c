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
static void *(*callocp)(size_t nmemb, size_t size) = NULL;
static void *(*reallocp)(void *ptr, size_t size) = NULL;

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
    // to avoid divide by 0
    unsigned long alloc_avg = n_malloc+n_calloc+n_realloc == 0 ?
                              0 : n_allocb/(n_malloc+n_calloc+n_realloc);

    LOG_STATISTICS(n_allocb, alloc_avg, n_freeb);
    LOG_STOP();

    // free list (not needed for part 1)
    free_list(list);
}

void* malloc(size_t size){
    char* error;
    mallocp = dlsym(RTLD_NEXT, "malloc");   // get addr of libc malloc

    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    void* ptr = mallocp(size);              // call libc malloc
    LOG_MALLOC(size, ptr);

    n_allocb += size;           // total allocated bytes
    n_malloc ++;                // total malloc call
    return ptr;
}

void* calloc(size_t nmemb, size_t size){
    char* error;
    callocp = dlsym(RTLD_NEXT, "calloc");   // get addr of libc calloc

    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    void* ptr = callocp(nmemb, size);       // call libc calloc
    LOG_CALLOC(nmemb, size, ptr);

    unsigned long csize = nmemb * size;
    n_allocb += csize;          // total allocated bytes
    n_calloc ++;                // total calloc call
    return ptr;
}

void* realloc(void* rptr, size_t size){
    char* error;
    reallocp = dlsym(RTLD_NEXT, "realloc"); // get addr of libc realloc

    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    void* ptr = reallocp(rptr, size);       // call libc realloc
    LOG_REALLOC(rptr, size, ptr);

    n_allocb += size;           // total allocated bytes
    n_realloc ++;               // total realloc call
    return ptr;
}

void free(void* ptr){
    if (!ptr){ return; }

    char* error;
    freep = dlsym(RTLD_NEXT, "free");       // Get address of libc free
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    freep(ptr);     // call libc realloc
    LOG_FREE(ptr);
}
