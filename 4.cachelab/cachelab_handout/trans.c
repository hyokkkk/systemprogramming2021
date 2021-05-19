//
// 안효지 2012-13311, stu3@sp1.snucse.org
//
// 설명은 코드 맨 하단에 있음. 자세한 내용은 리포트 참고.

/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

//int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void row_diff_col(int M, int N, int A[N][M], int B[M][N], int row, int col);
void row_eq_col(int M, int N, int A[N][M], int B[M][N], int row, int col);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32){
        // 8x8 matrix로 나눈다.
        for (int row = 0; row < N; row += 8){
            for (int col = 0; col < M; col += 8){
                if (row != col) { row_diff_col(M, N, A, B, row, col); }
                else { row_eq_col(M, N, A, B, row, col); }
            }
        }
    }else if (M == 61 && N == 67){
        for (int row = 0; row < N; row +=8){
            for (int col = 0; col < M; col +=8){
                row_diff_col(M, N, A, B, row, col);
            }
        }
    }else{
        for (int row = 0; row < N; row += 8){
            for (int col = 0; col < M; col += 8){
                // 2사분면의 원소 4개는 미리 받아놓고, 마지막에 B에 쓴다.
                int *tmp = &A[col][row];
                int a = tmp[0], b = tmp[1], c = tmp[2], d = tmp[3];

                // A의 1, 4사분면 -> B의 3, 4사분면
                for (int k = 0; k < 8; k++) {
                    int *tmp = &A[col + k][row + 4];
                    int a = tmp[0], b = tmp[1], c = tmp[2], d = tmp[3];
                    tmp = &B[row + 4][col + k];
                    tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                }
                // A의 3,2사분면 -> B의 1, 2 사분면
                for (int k = 7; k > 0; k--) {
                    int *tmp = &A[col + k][row];
                    int a = tmp[0], b = tmp[1], c = tmp[2], d = tmp[3];
                    tmp = &B[row][col + k];
                    tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                }
                // 아까 받아놓은 원소 4개 B에 씀.
                tmp = &B[row][col];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
            }
        }
    }
}

void row_eq_col(int M, int N, int A[N][M], int B[M][N], int row, int col)
{
    // 32x32는 row->col이 이득
    for (int i = row; i < row+8 && i < N; i++){
        int tmp;
        for (int j = col; j < col+8 && j < M; j++){
            if (i == j){
                tmp = A[i][j];
                continue; }
            B[j][i] = A[i][j];
        }
        // row = col 이면 miss/evict 줄이기 위해 나머지 다 끝나고나서 한다.
        // 그러면 그 다음 loop에서 hit을 만들 수 있음.
        B[i][i] = tmp;
    }
}

void row_diff_col(int M, int N, int A[N][M], int B[M][N], int row, int col)
{
    // 61 x 67때문에 j<M, i<N 조건을 넣어야 함.
    // col 먼저 돌 때가 더 efficient함.
    for (int j = col; j < col+8 && j < M; j++){
        for (int i = row; i < row+8 && i < N; i++){
            B[j][i] = A[i][j];
        }
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "4 element x 2 row(col)";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32){
        // 8x8 matrix로 나눈다.
        for (int row = 0; row < N; row += 8){
            for (int col = 0; col < M; col += 8){
                if (row != col) { row_diff_col(M, N, A, B, row, col); }
                else { row_eq_col(M, N, A, B, row, col); }
            }
        }
    }else if (M == 61 && N == 67){
        for (int row = 0; row < N; row +=8){
            for (int col = 0; col < M; col +=8){
                row_diff_col(M, N, A, B, row, col);
            }
        }
    }else{
        for (int row = 0; row < N; row += 8){
            for (int col = 0; col < M; col += 8){
                // A1->B3
                // 담고
                int *tmp = &A[col][row+4];
                int a = tmp[0], b = tmp[1], c = tmp[2], d = tmp[3];
                tmp = &A[col+1][row+4];
                int e = tmp[0], f = tmp[1], g = tmp[2], h = tmp[3];

                //옮기고
                tmp = &B[row+4][col];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;

                // 그 다음 두 줄 담고
                tmp = &A[col+2][row+4];
                a = tmp[0]; b = tmp[1]; c = tmp[2]; d = tmp[3];
                tmp = &A[col+3][row+4];
                e = tmp[0]; f = tmp[1]; g = tmp[2]; h = tmp[3];

                //옮기고
                tmp = &B[row+4][col+2];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;

                // A4->B4
                tmp = &A[col+4][row+4];
                a = tmp[0]; b = tmp[1]; c = tmp[2]; d = tmp[3];
                tmp = &A[col+5][row+4];
                e = tmp[0]; f = tmp[1]; g = tmp[2]; h = tmp[3];

                tmp = &B[row+4][col+4];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;

                tmp = &A[col+6][row+4];
                a = tmp[0]; b = tmp[1]; c = tmp[2]; d = tmp[3];
                tmp = &A[col+7][row+4];
                e = tmp[0]; f = tmp[1]; g = tmp[2]; h = tmp[3];

                tmp = &B[row+4][col+6];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;
                // A3->B1
                tmp = &A[col+4][row];
                a = tmp[0]; b = tmp[1]; c = tmp[2]; d = tmp[3];
                tmp = &A[col+5][row];
                e = tmp[0]; f = tmp[1]; g = tmp[2]; h = tmp[3];

                tmp = &B[row][col+4];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;

                tmp = &A[col+6][row];
                a = tmp[0]; b = tmp[1]; c = tmp[2]; d = tmp[3];
                tmp = &A[col+7][row];
                e = tmp[0]; f = tmp[1]; g = tmp[2]; h = tmp[3];

                tmp = &B[row][col+6];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;

                // A2->B2
                tmp = &A[col][row];
                a = tmp[0]; b = tmp[1]; c = tmp[2]; d = tmp[3];
                tmp = &A[col+1][row];
                e = tmp[0]; f = tmp[1]; g = tmp[2]; h = tmp[3];

                tmp = &B[row][col];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;

                tmp = &A[col+2][row];
                a = tmp[0]; b = tmp[1]; c = tmp[2]; d = tmp[3];
                tmp = &A[col+3][row];
                e = tmp[0]; f = tmp[1]; g = tmp[2]; h = tmp[3];

                tmp = &B[row][col+2];
                tmp[0] = a; tmp[64] = b; tmp[128] = c; tmp[192] = d;
                tmp[1] = e; tmp[65] = f; tmp[129] = g; tmp[193] = h;
            }
        }
    }

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

// <Explanation>
// 1. 32x32 matrix
//    - 아래 표는 32x32를 8x8로 쪼갠 것이다.
//    - miss/eviction이 일어나는 경우는 index가 같으나
//      tag가 다른 경우이다.
//    - cache set=32이므로 총 4개의 submatrix가 들어갈 수 있다.
//      (하나의 submatrix에는 8개의 set과, 각 set마다 1개의 blk가 있으므로)
//    - A[][], B[][]가 메모리상에 연속하여 위치하므로 주소를 계산하여 tag, index를
//      구하면 아래와 같다. 편의상 index는 mod 4한 값으로 표현한다.
//
//      A[][] (tag/index%4)            B[][] (tag/index%4)
//      _________________________      _________________________
//      | 0/0 | 0/1 | 0/2 | 0/3 |      | 4/0 | 4/1 | 4/2 | 4/3 |
//      |_____|_____|_____|_____|      |_____|_____|_____|_____|
//      | 1/0 | 1/1 | 1/2 | 1/3 |      | 5/0 | 5/1 | 5/2 | 5/3 |
//      |_____|_____|_____|_____|      |_____|_____|_____|_____|
//      | 2/0 | 2/1 | 2/2 | 2/3 |      | 6/0 | 6/1 | 6/2 | 6/3 |
//      |_____|_____|_____|_____|      |_____|_____|_____|_____|
//      | 3/0 | 3/1 | 3/2 | 3/3 |      | 7/0 | 7/1 | 7/2 | 7/3 |
//      |_____|_____|_____|_____|      |_____|_____|_____|_____|
//      (index%4=0 이면 index 0, 4, 8, 12, 16, 20, 24, 28이 들어있다는 의미)
//
//   1) 일단, 맨 위 행의 submatrix를 보면,
//      (0/0 -> 0/0), (0/1 -> 5/0), (0/2 -> 6/0), (0/3 -> 7/0)으로 옮겨지면 된다.
//      row==col 인 경우를 제외하고 tag는 다르지만 index는 같은 경우가 없으므로
//      eviction이 일어나지 않는다. 따라서 submatrix 상의 row != col인 경우에는
//      그냥 trans를 해도 된다.
//   2) row==col인 (0/0 -> 4/0), (1/1->5/1), (2/2 -> 6/2), (3/3 -> 7/3)의 경우엔
//      일단 원소가 row!=col한 경우는 trans하고, row=col의 경우는 index가 같으므로
//      불필요햔 eviction을 방지하기 위해 tmp에 받아놓는다.
//      2중 loop 중 deeper loop가 종료되면 B에 넣는다.
//
//           (tag:index)
//      A[0][0] (0:0) -> B[0][0] (4:0) : 이 때 B[0][0]에 값을 넣으면 eviction 발생.
//      A[0][1] (0:0) -> B[1][0] (4:4) : 그리고 다시 A[0][1]에 접근하면 또 eviction.
//      A[0][2] (0:0) -> B[2][0] (4:8)
//      A[0][3] (0:0) -> B[3][0] (4:12)
//                  ......
//
//      그러니 miss와 eviction을 줄이기 위해 B[0][0](4:0)에는 이 시점에서 값을 넣는다.
//
//      A[1][0] (0:4) -> B[0][1] (4:0) : 그럼 방금 (4:0)에 접근했었으니 여기서 hit가 됨.
//      A[1][1] (0:4) -> B[1][1] (4:4) : 이것도 맨 마지막에 넣음.
//      A[1][2] (0:4) -> B[2][1] (4:8)
//      A[1][3] (0:4) -> B[3][1] (4:12)
//                  ......
//
//
// 2. 61x67 matrix : 8x8 submatrix로 나누어서 돌리기만 해도 만점이 나온다.
//
//
// 3. 64x64 matrix
//  - 아래는 8x8 submatrix로 나눈 것을 다시 4x4로 나눈 그림이다.
//
//        |-(4byte)-|          |------(8byte)------|
//        ------------------------------------------
//        |<2> 0    | <1사분> ||    1    |         |
//        |    8    |         ||    9    |         |
//        |    16   |         ||    17   |         |
//        |    24   |         ||    25   |         |
//        ------------------------------------------
//        |<3> 0    | <4사분> ||    1    |         |
//        |    8    |         ||    9    |         |
//        |    16   |         ||    17   |         |
//        |    24   |         ||    25   |         |
//        ------------------------------------------
//
//  - cacheblock은 8byte이므로 2,1사분면/ 3,4사분면이 캐쉬 한 줄에 들어간다.
//  - 32x32에서는 row==col의 경우를 제외하고, input의 읽는 cache index와 output에 쓰는
//    cache index가 달랐기 때문에 원소 하나씩 읽고->쓰고->읽고->쓰고 순서대로 해도 됐다.
//    하지만 이 경우엔 1사분면의 원소 하나를 idx0에서 읽고, output 3사분면의 idx0에 쓰고,
//    input 1사분 idx0에 가서 그 다음 원소를 읽고, 또다시 output 3사분 idx8에 쓰는 행위를
//    반복하면, 불필요한 eviction이 많이 일어난다.
//  - 자세한 내용은 report 참고.
