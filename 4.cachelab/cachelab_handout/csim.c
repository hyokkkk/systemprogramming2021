/*
 2012-13311 안효지, stu3@sp1.snucse.org
*/

/*
 A cache is 2D array of cache lines: cache[S][E]
 S = 2^s : num of sets.
 E: num of lines per set(Associativity).
    ex1) direct mapped cache has only one line.
    ex2) 2-way associativity has two lines.
    ex3) 3-way associativity has three lines.
*/

/*
 <Overall structure>
              -----------
    cache --> | [set 1] | --> [line1], [line2], ... , [lineE]
              | [set 2] | --> [line1], [line2], ... , [lineE]
              |   ...   |
              | [set s] | --> [line1], [line2], ... , [lineE]
              -----------
*/


//#include <unistd.h>   //doesn't work on my env
#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>


// For our assignment, blk offsets are not used.
// It just simply counts hits, misses, and evictions.
typedef struct{
    bool valid;
    u_int64_t tag;
    size_t LRUcnt;
} Line;

typedef struct{
    Line* lines;
} Set;

typedef struct{
    Set* sets;
} Cache;


// cache data
Cache cache = {};
u_int64_t lineN, setN;
size_t index_bitN, offset_bitN;

// result data
size_t hit_cnt = 0,
       miss_cnt = 0,
       eviction_cnt = 0;

char opt_cnt = 0;       // for figuring out getting full options
bool verbose = false;

void printUsage();
void lookup(u_int64_t addr);
void LRUupdate(Set* set, Line* line);
void show();            // for convinient debugging.


int main(int argc, char** argv)
{
    size_t opt;
    FILE* filep = 0;

    //
    // 1. Parsing command line
    //
    while (-1 != (opt = getopt(argc, argv, "s:E:b:t:vh"))){
        switch(opt){
            case 's':
                index_bitN = (size_t)atoi(optarg);  // index bits == set bits
                setN = 1 << index_bitN;  // number of sets
                opt_cnt++; break;
            case 'E':
                lineN = (size_t)atoi(optarg);    // number of lines per set
                opt_cnt++; break;
            case 'b':
                offset_bitN = (size_t)atoi(optarg); // block bits == offset bits
                opt_cnt++; break;
            case 't':
                filep = fopen(optarg, "r"); // prepare for fscanf
                opt_cnt++; break;
            case 'v':
                verbose = true;
                break;
            case 'h':
            default:
                printUsage();
                return 1;
        }
    }
    if (!lineN || !index_bitN || !offset_bitN || !filep || opt_cnt != 4) {
        printf("./csim: Missing required command line argument\n");
        printUsage();
        return 1;
    }

    //
    // 2. Initialize cache
    // no need to init by 0. Below loop inits this by lineptr.
    //
    cache.sets = malloc(sizeof(Set) * setN);

    for (int i = 0; i < setN; ++i){
        // init by 0 is necessary.
        cache.sets[i].lines = calloc(sizeof(Line), lineN);
    }

    //
    // 3. Parse file input by fscanf
    //    - Reading lines like " M 20,1" or " L 19,3" or " S 14, 2"
    //    - Ignore "I".
    //    - M = L -> S
    //    - For this lab, single mem access never crosses block boundaries,
    //      therefore, we can ignore the request sizes.
    //
    char identifier;
    u_int64_t addr;   // 64bit memory address
    int size;

    while (fscanf(filep, " %c %lx,%d", &identifier, &addr, &size) > 0){
        switch (identifier){
            case 'L':
            case 'S':
                if (verbose) { printf("%c %lx,%d ", identifier, addr, size); }
                lookup(addr);
                //show();       // for convinient debugging
                if (verbose) { printf("\n"); }
                break;
            case 'M': // L -> S
                if (verbose) { printf("%c %lx,%d ", identifier, addr, size); }
                lookup(addr);
                lookup(addr);
                //show();       // for convinient debugging
                if (verbose) { printf("\n"); }
                break;
            case 'I':
                continue;
        }
    }

    //
    // 4. close and free
    //
    fclose(filep);
    for (int i = 0; i < setN; ++i){ free(cache.sets[i].lines); }
    free(cache.sets);

    printSummary(hit_cnt, miss_cnt, eviction_cnt);
    return 0;
}

/*
 addr : 64bit

 63          s+b          b           0
 --------------------------------------
 |    tag     |   index   |   offset  |
 --------------------------------------
*/
void lookup(u_int64_t addr){
    u_int64_t tag = addr >> (index_bitN+offset_bitN);
    u_int64_t index = (0xffffffffffffffff >> (64-index_bitN)) & (addr >> offset_bitN);

    // 1. index의 값을 이용해 해당하는 set을 선택한다.
    Set* set = &cache.sets[index];

    // 2. set의 line들을 loop 돌면서 tag, valid bit 확인
    for (u_int64_t i = 0; i < lineN; ++i){
        Line* line = &set->lines[i];
        if (!line->valid || line->tag != tag) { continue; }

    // 3. valid == 1, tag match : cache HIT
        if (verbose) { printf("hit "); }
        hit_cnt++;
        LRUupdate(set, line);
        return ;
    }

    // 4. cache miss : fine empty line -> valid == 0
    if (verbose) { printf("miss "); }
    miss_cnt++;

    for (u_int64_t i = 0; i < lineN; ++i){
        Line* line = &set->lines[i];
        if (line->valid){ continue; }

        // valid == false. Insert into the empty line.
        line->valid = true;
        line->tag = tag;
        LRUupdate(set, line);
        return ;
    }

    // 5. Replacement(eviction) by LRU: no empty line.
    if (verbose) { printf("eviction "); }
    eviction_cnt++;

    for (u_int64_t i = 0; i < lineN; ++i){
        Line* line = &set->lines[i];
        if (line->LRUcnt){ continue; }

        line->valid = true;
        line->tag = tag;
        LRUupdate(set, line);
        return ;
    }
}

/*
  <LRU(Least Recently Used)>
  - 현재 사용되고 있는 line의 LRU보다 큰 LRU들은 -1씩 한다.
  - 가장 최근에 사용된 것은 E-1의 값을 갖는다.

           1st  2nd  3rd  4th  5th
        L1  0  v 3    2  v 3    2
        L2  0    0  v 3    2    1
        L3  0    0    0    0  v 3
        L4  0    0    0    0    0
*/
void LRUupdate(Set* set, Line* line)
{
    for (u_int64_t i = 0; i < lineN; ++i){
        Line* lineItr = &set->lines[i];
        if (lineItr->LRUcnt <= line->LRUcnt){ continue; }
        (lineItr->LRUcnt)--;
    }
        line->LRUcnt = lineN-1;
}


void printUsage() {
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

// For debugging.
// Print overall structure.
void show() {
    for (int i = 0; i < setN; ++i){
        printf("<set%d>\n", i);
        for (int j = 0; j < lineN; ++j){
            Line* line = &cache.sets[i].lines[j];
            printf("[line%d]--[v:%d]-[tag:%lx]-[LRU:%lu]\t", j, line->valid,line->tag,line->LRUcnt);
        }
        printf("\n");
    }
}
