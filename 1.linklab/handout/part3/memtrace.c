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

    // only print when nonfreed bytes != 0
    if (n_allocb != n_freeb){
        LOG_NONFREED_START();
        item* l = list;
        while(l != NULL){
            if (l->cnt != 0){
                LOG_BLOCK(l->ptr, l->size, l->cnt);
            }
            l = l->next;
        }
    }
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
    mallocp = dlsym(RTLD_NEXT, "malloc");   // get addr of libc malloc

    void* ptr = mallocp(size);              // call libc malloc
    LOG_MALLOC(size, ptr);

    alloc(list, ptr, size);     // insert in the list
    n_allocb += size;           // total allocated bytes
    n_malloc ++;                // total malloc call
    return ptr;
}

void* calloc(size_t nmemb, size_t size){
    char* error;
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    callocp = dlsym(RTLD_NEXT, "calloc");   // get addr of libc calloc

    void* ptr = callocp(nmemb, size);       // call libc calloc
    LOG_CALLOC(nmemb, size, ptr);

    unsigned long csize = nmemb * size;
    alloc(list, ptr, csize);    // insert in the list
    n_allocb += csize;          // total allocated bytes
    n_calloc ++;                // total calloc call
    return ptr;
}

void* realloc(void* rptr, size_t size){
    char* error;
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    reallocp = dlsym(RTLD_NEXT, "realloc"); // get addr of libc realloc

    void* ptr = reallocp(rptr, size);       // call libc realloc
    LOG_REALLOC(rptr, size, ptr);

    n_freeb += dealloc(list, rptr)->size;   // dealloc & count freed bytes
    alloc(list, ptr, size);     // alloc new size & pointer
    n_allocb += size;           // total allocated bytes
    n_realloc ++;               // total realloc call
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
    LOG_FREE(ptr);

    // to check illigal free and double free
    item* target = find(list, ptr);
    if (!target){ LOG_ILL_FREE(); }
    else if(target->cnt == 0){ LOG_DOUBLE_FREE(); }
    else {
        n_freeb += dealloc(list, ptr)->size;    //freed bytes
        freep(ptr);                             // call libc realloc
    }
}
