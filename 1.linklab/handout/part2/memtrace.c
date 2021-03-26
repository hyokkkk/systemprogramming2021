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

    // only printf when nonfreed bytes != 0
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

//----------------------------------------------
void* malloc(size_t size){
    char* error;
    mallocp = dlsym(RTLD_NEXT, "malloc");   // get addr of libc malloc

    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    if (size == 0){
        LOG_MALLOC(size, NULL);
        return NULL;
    }else{
        void* ptr = mallocp(size);              // call libc malloc
        LOG_MALLOC(size, ptr);

        alloc(list, ptr, size);                 // insert in the list
        n_allocb += size;                       // total allocated bytes
        n_malloc ++;                            // total malloc call
        return ptr;
    }
}

//----------------------------------------------
void* calloc(size_t nmemb, size_t size){
    char* error;
    callocp = dlsym(RTLD_NEXT, "calloc");   // get addr of libc calloc

    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    // malloc에 이어 0인 경우에는 그냥 null return하게 해놓음. either이니까.
    if (nmemb == 0 | size == 0){
        LOG_CALLOC(nmemb, size, NULL);
        return NULL;
    }else {
        void* ptr = callocp(nmemb, size);       // call libc calloc
        LOG_CALLOC(nmemb, size, ptr);

        unsigned long csize = nmemb * size;
        alloc(list, ptr, csize);                // insert in the list
        n_allocb += csize;                      // total allocated bytes
        n_calloc ++;                            // total calloc call
        return ptr;
    }
}

/*****************************************************************/
/* 1. old size >= new size : 아무 일도 일어나지 않는다.          */
/* 2. old size < new size : always dealloc first, and then alloc */
/* 3. log is updated only when alloc/dealloc has done            */
/*****************************************************************/
void* realloc(void* ptr, size_t size){
    char* error;
    reallocp = dlsym(RTLD_NEXT, "realloc"); // get addr of libc realloc

    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    void* rptr = reallocp(ptr, size); // call libc realloc
    item* target = find(list, ptr);

    // size 0 인 경우 free(ptr)처럼 작동.
    if (size == 0) {
        char* ferror;
        freep = dlsym(RTLD_NEXT, "free");       // Get address of libc free
        if ((ferror = dlerror()) != NULL){
            fputs(ferror, stderr);
            exit(1);
        }
        LOG_REALLOC(ptr, size, NULL);
        n_freeb += dealloc(list, ptr)->size;    // freed bytes
        freep(ptr);                             // call libc free
        return NULL;
    // normal ptr & size
    }else {
        LOG_REALLOC(ptr, size, rptr);
        if (target->size < size){
            n_freeb += dealloc(list, ptr)->size;// dealloc & count freed bytes
            alloc(list, rptr, size);            // alloc new size & pointer
            n_allocb += size;                   // total allocated bytes
            n_realloc ++;                       // total realloc call
        }
        return rptr;
    }
}

//----------------------------------------------
void free(void* ptr){
    if (!ptr){ return; }

    char* error;
    freep = dlsym(RTLD_NEXT, "free");       // Get address of libc free
    if ((error = dlerror()) != NULL){
        fputs(error, stderr);
        exit(1);
    }
    n_freeb += dealloc(list, ptr)->size;    //freed bytes
    freep(ptr);                             // call libc realloc
    LOG_FREE(ptr);
}
