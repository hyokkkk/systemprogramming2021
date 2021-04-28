/*
 *
 *
 * implicit list로 구현하였다.
 *
 *
 *
 * */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE           4           /* Word and header/footer size (bytes) */
#define DSIZE           8           /* Double word size (bytes) */
#define CHUNKSIZE       (1<<12)     /* Extend heap by this amount (bytes) */
#define MAX(x, y)       ((x) > (y) ? (x) : (y))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT       DSIZE

/* Pack a size and allocated bit into a word*/
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int*)(p))
#define PUT(p, val)     (*(unsigned int*)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)         // ~0x7 : 000
#define GET_ALLOC(p)    (GET(p) & 0x1)          // LSB = alloc bit

/* Given block ptr bp, compute address of its header and footer */
// char*로 casting해야 1byte씩 계산 됨.
#define HDRP(bp)        ((char*)(bp) - WSIZE)
#define FTRP(bp)        ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE )

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ( (char*)(bp) + GET_SIZE( ((char*)(bp)-WSIZE) ) )
#define PREV_BLKP(bp)   ( (char*)(bp) - GET_SIZE( ((char*)(bp)-DSIZE) ) )


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)     ( ((size)+(ALIGNMENT-1)) & ~0x7 )

#define SIZE_T_SIZE     (ALIGN(sizeof(size_t)))


static void place(void* bp, size_t size);
static void* heap_listp = 0;
static void* extend_heap(size_t wordscnt);
static void* coalescse(void* bp);
static void* find_fit(size_t size);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1){
        return -1;
    }
    //                    r--- heap_listp
    //--------------------V------------------------
    // unused | prolH 8/1 | prolF 8/1 | epilH 0/1 |
    //---------------------------------------------
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     // epilogue header
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {return -1;}

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t adjsize;
    size_t extendsize;
    char* bp;

    /* Ignore squrious requests */
    if (size == 0){ return NULL; }

    /* Adjust block size to include overhead and alignment reqs. */
    // why DSIZE? : header(4byte) + footer(4byte)
    adjsize = ALIGN(size + DSIZE);

    /* search the free list for a fit */
    if ((bp = find_fit(adjsize)) != NULL) {
        place(bp, adjsize);
        return bp;
    }

    /* No fit found. get more memory and place the block */
    extendsize = MAX(adjsize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, adjsize);
    return bp;
}

static void place(void* bp, size_t adjsize){
    /* get the extended size and decide splitting or not */
    size_t extsize = GET_SIZE(HDRP(bp));

    /* the minimum block size is 16bytes
     * - ALIGN(hdr 4by + ftr 4b + min 1byte) */
    // if there are enough space for new block
    if ((extsize - adjsize) >= 2*ALIGNMENT){
        // split
        PUT(HDRP(bp), PACK(adjsize, 1));
        PUT(FTRP(bp), PACK(adjsize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(extsize - adjsize, 0));
        PUT(FTRP(bp), PACK(extsize - adjsize, 0));
    // not enough space
    }else {
        PUT(HDRP(bp), PACK(extsize, 1));
        PUT(FTRP(bp), PACK(extsize, 1));
    }
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalescse(bp);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void *oldbp= bp;
    void *newbp;
    size_t adjsize = ALIGN(size+DSIZE);
    size_t oldsize = GET_SIZE(HDRP(bp));

    // 0. if size == 0 -> mem_free()
    if (size == 0) {
        mm_free(bp);
        return bp;
    }
    // 1. compare size
    if (GET_SIZE(HDRP(oldbp)) == adjsize){
        return oldbp;
    }else if (GET_SIZE(HDRP(oldbp)) > adjsize){
    // 사이즈 작은 경우
        PUT(HDRP(oldbp), PACK(adjsize, 1)); /* update header */
        PUT(FTRP(oldbp), PACK(adjsize, 1)); /* update footer */
        PUT(HDRP(NEXT_BLKP(oldbp)), PACK(oldsize-adjsize, 0)); /* update nextblk hdr */
        PUT(FTRP(NEXT_BLKP(oldbp)), PACK(oldsize-adjsize, 0)); /* update nextblk ftr */
        coalescse(NEXT_BLKP(oldbp));
        return oldbp;
    }else {
    // 사이즈 더 큰 경우
        // 뒤에 충분한 공간이 있을 경우
        size_t nextsize = GET_SIZE(HDRP(NEXT_BLKP(oldbp)));
        size_t addedsize = adjsize - oldsize;
        if ((!GET_ALLOC(HDRP(NEXT_BLKP(oldbp)))) && nextsize >= addedsize){
            PUT(FTRP(oldbp), 0);                   /* delete bp ftr */
            PUT(FTRP(NEXT_BLKP(oldbp)), PACK(nextsize-addedsize, 0));/* set nextblk ftr */
            PUT(HDRP(NEXT_BLKP(oldbp)), 0);        /* delete nextblk hdr */
            PUT(HDRP(oldbp), PACK(adjsize, 1));    /* update header*/
            PUT(FTRP(oldbp), PACK(adjsize, 1));    /* update ftr */
            PUT(HDRP(NEXT_BLKP(oldbp)), PACK(nextsize-addedsize, 0));/* set nextblk hdr */
            return oldbp;
        // 뒤에 공간이 없어서 다른 곳을 찾아야 하는 경우
        }else {
            newbp = mm_malloc(size);
            memcpy(newbp, oldbp, oldsize-DSIZE);
            mm_free(oldbp);
            return newbp;
        }
    }
}

static void* extend_heap(size_t wordscnt)
{
    char* bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (wordscnt % 2) ? (wordscnt+1) * WSIZE : wordscnt * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    /* Initialize free block header/foter and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));           // free blk header
    PUT(FTRP(bp), PACK(size, 0));           // free blk footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // new epilog header

    /* Coalesce if the previous block was free */
    return coalescse(bp);
}

static void* coalescse(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp; }
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void* find_fit(size_t adjsize)
{
    /* first fit search */
    void* bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0;
            bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && adjsize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;
}


