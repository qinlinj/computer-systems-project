/**
 * @file trans.c
 * @brief Contains various implementations of matrix transpose
 *
 * Each transpose function must have a prototype of the form:
 *   void trans(size_t M, size_t N, double A[N][M], double B[M][N],
 *              double tmp[TMPCOUNT]);
 *
 * All transpose functions take the following arguments:
 *
 *   @param[in]     M    Width of A, height of B
 *   @param[in]     N    Height of A, width of B
 *   @param[in]     A    Source matrix
 *   @param[out]    B    Destination matrix
 *   @param[in,out] tmp  Array that can store temporary double values
 *
 * A transpose function is evaluated by counting the number of hits and misses,
 * using the cache parameters and score computations described in the writeup.
 *
 * Programming restrictions:
 *   - No out-of-bounds references are allowed
 *   - No alterations may be made to the source array A
 *   - Data in tmp can be read or written
 *   - This file cannot contain any local or global doubles or arrays of doubles
 *   - You may not use unions, casting, global variables, or
 *     other tricks to hide array data in other forms of local or global memory.
 *
 * TODO: fill in your name and Andrew ID below.
 * @author Qinlin Jia <qinlinj@andrew.cmu.edu>
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#include "cachelab.h"

/**
 * @brief Checks if B is the transpose of A.
 *
 * You can call this function inside of an assertion, if you'd like to verify
 * the correctness of a transpose function.
 *
 * @param[in]     M    Width of A, height of B
 * @param[in]     N    Height of A, width of B
 * @param[in]     A    Source matrix
 * @param[out]    B    Destination matrix
 *
 * @return True if B is the transpose of A, and false otherwise.
 */
#ifndef NDEBUG
static bool is_transpose(size_t M, size_t N, double A[N][M], double B[M][N]) {
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                fprintf(stderr,
                        "Transpose incorrect.  Fails for B[%zd][%zd] = %.3f, "
                        "A[%zd][%zd] = %.3f\n",
                        j, i, B[j][i], i, j, A[i][j]);
                return false;
            }
        }
    }
    return true;
}
#endif

/*
 * You can define additional transpose functions here. We've defined
 * some simple ones below to help you get started, which you should
 * feel free to modify or delete.
 */

/**
 * @brief A simple baseline transpose function, not optimized for the cache.
 *
 * Note the use of asserts (defined in assert.h) that add checking code.
 * These asserts are disabled when measuring cycle counts (i.e. when running
 * the ./test-trans) to avoid affecting performance.
 */
static void trans_basic(size_t M, size_t N, double A[N][M], double B[M][N],
                        double tmp[TMPCOUNT]) {
    assert(M > 0);
    assert(N > 0);

    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; j++) {
            B[j][i] = A[i][j];
        }
    }

    assert(is_transpose(M, N, A, B));
}

/**
 * @brief A contrived example to illustrate the use of the temporary array.
 *
 * This function uses the first four elements of tmp as a 2x2 array with
 * row-major ordering.
 */
static void trans_tmp(size_t M, size_t N, double A[N][M], double B[M][N],
                      double tmp[TMPCOUNT]) {
    assert(M > 0);
    assert(N > 0);

    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; j++) {
            size_t di = i % 2;
            size_t dj = j % 2;
            tmp[2 * di + dj] = A[i][j];
            B[j][i] = tmp[2 * di + dj];
        }
    }

    assert(is_transpose(M, N, A, B));
}

/**
 * Transposes a 32x32 matrix.
 * This function is optimized for a 32x32 matrix and uses a block-wise strategy
 * to enhance performance by maximizing spatial locality and minimizing cache
 * misses.
 *
 * @param M: Width of the matrix A and height of matrix B.
 * @param N: Height of the matrix A and width of matrix B.
 * @param A: Source matrix (Input).
 * @param B: Destination matrix where the transposed matrix will be stored
 * (Output).
 */
static void transpose_32x32(size_t M, size_t N, double A[N][M], double B[M][N],
                            double tmp[TMPCOUNT]) {
    // Define the size of blocks. The choice of 8x8 is strategic for cache
    // utilization.
    const size_t BLOCK_SIZE = 8;
    size_t i, j, ii, jj;
    size_t block_row, block_col;

    for (block_row = 0; block_row < N; block_row += BLOCK_SIZE) {
        for (block_col = 0; block_col < M; block_col += BLOCK_SIZE) {
            // Transpose each block
            for (i = block_row; i < block_row + BLOCK_SIZE; ++i) {
                for (j = block_col; j < block_col + BLOCK_SIZE; ++j) {
                    if (i != j) {
                        // If the element is not on the diagonal, transpose it
                        // directly.
                        B[j][i] = A[i][j];
                    } else {
                        // For diagonal elements, store the indices to handle
                        // them after this inner loop.
                        ii = i;
                        jj = j;
                    }
                }
                // After processing a row within a block, handle the diagonal
                // element if the block is on the main diagonal.
                if (block_row == block_col) {
                    B[jj][ii] = A[ii][jj];
                }
            }
        }
    }
}

/**
 * Transposes a 1024x1024 matrix.
 * This function is optimized for a 1024x1024 matrix using a block-wise strategy
 * to optimize cache usage. The goal is to maximize spatial locality and
 * minimize cache misses. Focus on reducing conflict misses along the diagonal
 * by storing diagonal elements temporarily in the tmp array.
 *
 * @param M: Width of the matrix A and height of matrix B.
 * @param N: Height of the matrix A and width of matrix B.
 * @param A: Source matrix (Input).
 * @param B: Destination matrix where the transposed matrix will be stored
 * (Output).
 * @param tmp: Temporary storage which can be used for caching to optimize cache
 * performance.
 */
static void transpose_1024x1024(size_t M, size_t N, double A[N][M],
                                double B[M][N], double tmp[TMPCOUNT]) {
    // Set block size to 8x8 for our transpose operation
    const size_t BLOCK_SIZE = 8;

    for (size_t ii = 0; ii < N; ii += BLOCK_SIZE) {
        for (size_t jj = 0; jj < M; jj += BLOCK_SIZE) {

            // Inner two loops handle the transpose of each individual block.
            for (size_t i = ii; i < ii + BLOCK_SIZE; ++i) {
                for (size_t j = jj; j < jj + BLOCK_SIZE; ++j) {

                    // not a diagonal element, transpose normally.
                    if (i != j) {
                        B[j][i] = A[i][j];
                    } else {
                        // diagonal element, store it temporarily in tmp to
                        // avoid cache conflict misses.
                        tmp[i - ii] = A[i][j];
                    }
                }

                // within a diagonal block, restore the diagonal elementsfrom
                // tmp to the transposed position in B.
                if (ii == jj) {
                    B[i][i] = tmp[i - ii];
                }
            }
        }
    }
}

static void transpose_submit(size_t M, size_t N, double A[N][M], double B[M][N],
                             double tmp[TMPCOUNT]) {
    if (M == 32 && N == 32) {
        transpose_32x32(M, N, A, B, tmp);
    } else if (M == 1024 && N == 1024) {
        transpose_1024x1024(M, N, A, B, tmp);
    } else {
        // For other sizes.
        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < M; j++) {
                B[j][i] = A[i][j];
            }
        }
    }
}

/**
 * @brief Registers all transpose functions with the driver.
 *
 * At runtime, the driver will evaluate each function registered here, and
 * and summarize the performance of each. This is a handy way to experiment
 * with different transpose strategies.
 */
void registerFunctions(void) {
    // Register the solution function. Do not modify this line!
    registerTransFunction(transpose_submit, SUBMIT_DESCRIPTION);

    // Register any additional transpose functions
    registerTransFunction(trans_basic, "Basic transpose");
    registerTransFunction(trans_tmp, "Transpose using the temporary array");
}
