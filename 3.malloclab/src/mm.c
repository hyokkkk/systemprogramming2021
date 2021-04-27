/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

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
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int*)(p))
#define PUT(p, val)     (*(unsigned int*)(p) = (val))

/* surf: list의 edge 중, 위쪽, 혹은 옆으로만 가리키는 아이들 */
/* deep: list의 edge 중, 아래쪽만 가리키는 아이들 */
/* 다음 blk의 pointer를 리턴한다. */
//TODO: 잘 돌아가나 확인해야 함.
#define GET_SURFV(bp)    ((unsigned int*)((*(unsigned int*)(bp)) & ~0x7))
#define GET_DEEPV(bp)    ((unsigned int*)((*((unsigned int*)DEEPP(bp))) & ~0x7))

/* surf, deep의 기존 lsb3bit는 살린채로 가리켜야 할 blk 주소를 넣는다. */
#define SET_LINK(p, tgt)   PUT(p, ((unsigned int)((unsigned int*)(tgt)))\
                           | ((unsigned int)((unsigned int*)(GET_CLSINFO(p)))))

/* Given block ptr bp, compute address of it surf and deep */
#define SURFP(bp)       ((char*)(bp))
#define DEEPP(bp)       ((char*)(bp) + WSIZE)

/* size class info를 free block 안에 넣는다. */
//TODO: 크기 작은 것 더 세분화해서 넣기.
//그러려면 deep << 3해서 더해야 함.
#define GET_CLSINFO(bp)  ((GET(bp) & 0x7) + (((GET((char*)bp + WSIZE)) & 0x7)<<3))
#define PUT_CLSINFO(p, info)    (*(unsigned int*)(p) = (((GET(p)) & ~0x7) | (info)))

/* write main stream blk or not */
// free list 중 main stream에 흐르는 blk인지 아닌지 표시한다.
// hdr, ftr 따로따로 적어줘야 함.
#define WRT_MAINST(p, x)        PUT(p, GET(p) | (x<<1))
#define IS_MAINST(bp)           ((GET(HDRP(bp)) & 2) >> 1)

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
static void* extend_heap(size_t wordscnt);
static void* coalescse(void* bp);
static void* find_fit(size_t size);
static int decide_sizeclass(size_t adjsize);
static void write_sizeclass(void* bp, int class);

static void* heap_listp = 0;
static void* free_listp = 0;
static void* prevp;
/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    // free_listp의 dummy blk: 2WORDS 해도 에러 안 생기나 봐.
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void*)-1){
        return -1;
    }
//                heap_listp    free_listp
//--------------------|-----------|-----------------------------
// unused | prolH 8/1 | prolF 8/1 | surface | deep | epilH 0/1 |
//--------------------------------------------------------------
    PUT(heap_listp, 0);             // unused
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // prologue footer
    PUT(heap_listp + (3*WSIZE), 0);     // surface init 0
    PUT(heap_listp + (4*WSIZE), 0);     // deep init 0
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));     // epilogue header
    heap_listp += (2*WSIZE);
    free_listp = heap_listp + WSIZE;
    //TODO:
    printf("\n***heaplistp : %p\n", heap_listp);
    printf("***freelistp : %p\n", free_listp);

    void* rgbp;     //reg blk ptr
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ((rgbp = extend_heap(CHUNKSIZE/WSIZE)) == NULL) {return -1;}

//heap_listp  free_listp """"""""""""""""| rgbp
//----|-----------|------"---------------v--------------------------------
//    | prolF 8/1 | surface | deep | hdr | regular blk | ftr | epilH 0/1 |
//------------------------------------------------------------------------
// free_listp의 dummy block의 surface가 freep를 가리키도록 한다.
    SET_LINK(SURFP(free_listp), rgbp);
    WRT_MAINST(HDRP(rgbp), 1);      // 처음 생긴 free list니까
    WRT_MAINST(FTRP(rgbp), 1);      // 당연히 main stream이다.

    //TODO: reg blk의 size 정보
    printf("***reg blk size info: %d\n", GET_CLSINFO(rgbp));
    //TODO: freelist dummy blk이 reg blk 잘 가리키는지
    printf("***reg blk pointer: %p\n", rgbp);
    printf("***surf가 가리키는 주소: %p\n", GET_SURFV(free_listp));
    printf("***deep가 가리키는 주소: %p\n", GET_DEEPV(free_listp));
    printf("***mainstream이냐? %d\n", IS_MAINST(rgbp));

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


    printf("\nmalloc이다\n");
    /* search the free list for a fit */
    if ((bp = find_fit(adjsize)) != NULL) {
        // place에서 adjsize를 bp에 놓음.
        // decide_sizeclass는 사이즈를 받아서 그 사이즈가 어느
        // 사이즈 클래스에 있는지 알려줘야 함?
        printf("findfit에서는 나옴. place들어갈차례\n");
        place(bp, adjsize);
    //TODO: delete
    //printf("bp 주소: %p\n", bp);
        return bp;
    }

    printf("----findfit 실패함\n");
    /* No fit found. get more memory and place the block */
    extendsize = MAX(adjsize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, adjsize);
    //TODO: delete
    //printf("bp 주소: %p\n", bp);
    return bp;
//    int newsize = ALIGN(size + SIZE_T_SIZE);
//    void *p = mem_sbrk(newsize);
//    if (p == (void *)-1)
//	return NULL;
//    else {
//        *(size_t *)p = size;
//        return (void *)((char *)p + SIZE_T_SIZE);
//    }
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
    // 사이즈 더 큰 경우
    }else {
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
    printf("\n-------extend_heap 들어옴\n");
    char* bp;
    size_t adjsize;

    /* Allocate an even number of words to maintain alignment */
    adjsize = (wordscnt % 2) ? (wordscnt+1) * WSIZE : wordscnt * WSIZE;
    //printf("adjsize: %d\n",(int)size);
    //TODO: 왜 long이지?
    if ((unsigned int)(bp = mem_sbrk(adjsize)) == -1){
        return NULL;
    }

    // sbrk은 이전 heap의 마지막 위치를 return한다.
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(adjsize, 0));           // free blk header(old epilog)
    PUT(FTRP(bp), PACK(adjsize, 0));           // free blk footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // new epilog header
    printf("size test:%d\n", decide_sizeclass(adjsize));

    // decide sizeclass
    write_sizeclass(bp,decide_sizeclass(adjsize));

    /* Coalesce if the previous block was free */
    return coalescse(bp);
}

// write sizeclass into the bp blk
// surf LSB3bit, deep LSB3bit
static void write_sizeclass(void* bp, int class)
{
        PUT_CLSINFO(SURFP(bp), class%8);
        PUT_CLSINFO(DEEPP(bp), class/8);
}



static int decide_sizeclass(size_t adjsize)
{
    // 원래는 sizeclass index의 값을 blk안에 적어놓지 않고
    // log(GET_SIZE(bp))/log(2) 값을 구해서 alloc/free되는 즉시
    // list에 insert/delete를 하고 싶었다. 그러나 log(x) 사용시
    // x 안에 상수가 들어오면 컴파일 시 -lm이 필요없지만,
    // 값이 정해져있지 않은 size를 x에 넣으면 꼭 -lm을 넣어야
    // 오류가 생기지 않고 컴파일이 되는 것을 발견하였다.
    // 슬프지만 mm.c의 interface 및 mm.c 이외의 파일은 수정하면
    // 안 되므로 이런 노가다 방법을 사용하였다.
    printf("---------class 정하러 들어옴.\n adjsize : %d\n", (unsigned int)adjsize);

    if (adjsize <= 1<<6){ return 0; }
    else if (adjsize <= 1<<7){ return 1; }
    else if (adjsize <= 1<<8){ return 2; }
    else if (adjsize <= 1<<9){ return 3; }
    else if (adjsize <= 1<<10){ return 4; }
    else if (adjsize <= 1<<11){ return 5; }
    else if (adjsize <= 1<<12){ return 6; }
    else if (adjsize <= 1<<13){ return 7; }
    else if (adjsize <= 1<<14){ return 8; }
    else if (adjsize <= 1<<15){ return 9; }
    else if (adjsize <= 1<<16){ return 10; }
    else if (adjsize <= 1<<17){ return 11; }
    else if (adjsize <= 1<<18){ return 12; }
    else if (adjsize <= 1<<19){ return 13; }
       // 최대 입력 숫자는 6자리임.
    else { return 14; }
}


static void* coalescse(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }else if (prev_alloc && !next_alloc){
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
    printf("find fit 들어옴\n");
    //TODO: best fit
    /* first fit search */
    prevp = free_listp;
    void* bp = GET_SURFV(free_listp);

    // decide는 bp의 사이즈를 찾아서 어느 클래스에 속하는지
    // 알려준다.
    //TODO:
    // blk에 classinfo를 쓰지 않고 사이즈만 갖고 클래스 비교하는
    // 것이 필요함.
    int adjsizeclass = decide_sizeclass(adjsize);
    // adjclass가 더 클 동안 mainstream 돌면서 같은게 나오면 일단 멈춰
    while (bp != NULL){
        int blkclass = GET_CLSINFO(bp);
        // 나랑 같은 거 찾거나 같은 거 없으면 어쨌든 큰거에서 찾는다.
        if (adjsizeclass < blkclass){
            return bp;
        }else if (adjsizeclass == blkclass){
        // 사이즈 같은 경우임. 내 실제 사이즈와 같거나, 큰 녀석들 있나 봐야 함.
            void* bp2 = GET_DEEPV(bp);
            void* bp3;
            while (bp2 != NULL){
                // null이면 바로 이 루프 깨짐
                if (adjsize <= GET_SIZE(bp2)){
                    return bp2;
                }
                // 나보다는 작은 경우
                bp2 = GET_DEEPV(bp2);
            }
            // main stream 하나밖에 없는데 얘가 나보다 같거나 크면 애로,
            // 아니면 그 다음 크기로.
            if (GET_SIZE(bp) >= adjsize) {
                return bp;
            }else if ((bp3 = GET_SURFV(bp)) != NULL){
                prevp = bp;
                return bp3;
            }
        }
        // 계속 진행함
        prevp = bp;
        //TODO: 여기가 겹치긴 함.
        bp = GET_SURFV(bp);
    }
    return NULL;
}

    // 하면 bp가 여기 나와있음.
    //이제 해당 bp에 들어가서 걍 일단 firstfit으로 찾아보자. bestfit은 시간이
    //   너무 걸릴 거 같음.;; 그럼 걍 mainstream을 고르면 되는 거 아녀.

    // 아냐. 여기 나오면 null인 거야. 나랑 같은 거도 없고, 나보다 큰 것도 없어.
    // sbrk해야 해.
//    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0;
//            bp = NEXT_BLKP(bp)){
//        if (!GET_ALLOC(HDRP(bp)) && adjsize <= GET_SIZE(HDRP(bp))){
//            return bp;
//        }
//    }

static void place(void* bp, size_t adjsize){
    printf("place들어왓쪄여\n");
    printf("===아래 둘이 같아야 함 ===\n");
    printf("bp: %p\n", bp);
    printf("freelistp's surf: %p\n", GET_SURFV(free_listp));
    printf("============================\n");
    printf("prev는 freelistp여야 함: %p\n", prevp);
    // 1. bp이 mainstream인지 아닌지 확인
    // 1-1. main stream이라면?
    //      -> prevp.surf = bp.deep
    //      -> prevp.surf.surf = bp.surf
    //      -> 혹시모르니 bp.surf = 0 , bp.deep = 0
    // 1-2. deep이라면
    //      -> bp.surf.deep = bp.deep
    //      -> bp.deep.surf = bp.surf
    //      -> 혹시모르니 bp.surf = 0. bp.deep = 0
    //
    // 2. 잘려진 부분 적당한 list에 넣기
    //      -> free_listp 받아서 loop돈다
    //      -> 내 클래스 == 리스트 클래스: 첫번째 deep으로 들어감. prevp필요x
    //      -> 내클래서 < 리스트클래스 : 내 클래스가 없다는 의미.prevp필요o
    //
    /* get the extended size and decide splitting or not */
    size_t extsize = GET_SIZE(HDRP(bp));
    if (GET_ALLOC(bp)){
        printf("alloc된 블록이라 에러났음!\n");
        return ;
    }

    /* the minimum block size is 16bytes
     * - ALIGN(hdr 4by + ftr 4b + min 1byte) */
    // if there is enough space for new block
    if ((extsize - adjsize) >= 2*ALIGNMENT){
        // split
        if (IS_MAINST(bp)){
            // 1-1.
            SET_LINK(SURFP(prevp), GET_DEEPV(bp));
            if (GET_DEEPV(bp) != NULL){
                WRT_MAINST(HDRP(GET_DEEPV(bp)), 1);       // mainstream 표시
                WRT_MAINST(FTRP(GET_DEEPV(bp)), 1);       // mainstream 표시
            SET_LINK(SURFP(GET_SURFV(prevp)), GET_SURFV(bp));
            SET_LINK(SURFP(bp), 0);
            SET_LINK(DEEPP(bp), 0);
            WRT_MAINST(HDRP(bp), 0);          // no more main stream
            WRT_MAINST(FTRP(bp), 0);          // no more main stream
            }
        }else{
            // 1-2.
            SET_LINK(DEEPP(GET_SURFV(bp)), GET_DEEPV(bp));
            SET_LINK(SURFP(GET_DEEPV(bp)), GET_DEEPV(bp));
            SET_LINK(SURFP(bp), 0);
            SET_LINK(DEEPP(bp), 0);
        }
        PUT(HDRP(bp), PACK(adjsize, 1));
        PUT(FTRP(bp), PACK(adjsize, 1));
        // 이 위까지가 free->alloc blk free list에서 삭제
        // 아래부터는 나머지부분 class에 맞게 free list 새롭게 insert
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(extsize - adjsize, 0));
        PUT(FTRP(bp), PACK(extsize - adjsize, 0));

        unsigned int* mitr = GET_SURFV(free_listp); // main stream itr
        printf("mitr null나와야하는디?: %p\n", mitr);
        write_sizeclass(bp, decide_sizeclass(adjsize));
        int bpclass = GET_CLSINFO(bp);
        prevp = free_listp;
        while (mitr != NULL){
            int msclass = GET_CLSINFO(mitr); // mainstream 블럭의 class info
            if (bpclass == msclass){
                SET_LINK(SURFP(bp), GET_DEEPV(GET_SURFV(mitr)));
                SET_LINK(DEEPP(bp), GET_DEEPV(GET_SURFV(mitr)));
                SET_LINK(SURFP(GET_DEEPV(mitr)), bp);
                SET_LINK(DEEPP(mitr), bp);
            } else if (bpclass < msclass){
                WRT_MAINST(HDRP(bp), 1);
                WRT_MAINST(FTRP(bp), 1);
                // bp.surf = prev.surf
                // prev.surf = bp
                SET_LINK(SURFP(bp), GET_SURFV(prevp));
                SET_LINK(SURFP(prevp), bp);
            }
            prevp = bp;
            bp = GET_SURFV(bp);
        }

    // not enough space
    }else {
        // 8 or 0 byte 남았으면 걍 internal fragment로 남겨두는 게 나음.
        // 나중에 free되고 coalescing 될 때 같이 되면 됨.
        PUT(HDRP(bp), PACK(extsize, 1));
        PUT(FTRP(bp), PACK(extsize, 1));
    }
}

