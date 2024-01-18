/**
 * @file mm.c
 * @brief A 64-bit struct-based implicit free list memory allocator
 *
 * 15-213: Introduction to Computer Systems
 * This implementation of memory allocation utilizes an explicit approach.
 * It employs a first-fit strategy to find suitable memory blocks and maintains
 *a doubly linked, null-terminated free list. Memory blocks are tracked using
 *8-byte headers and footers, with allocated blocks still retaining footers. The
 *insertion method into the free list follows a Last-In, First-Out (LIFO)
 *strategy, ensuring efficient utilization of available memory.
 *
 *************************************************************************
 *
 * ADVICE FOR STUDENTS.
 * - Step 0: Please read the writeup!
 * - Step 1: Write your heap checker.
 * - Step 2: Write contracts / debugging assert statements.
 * - Good luck, and have fun!
 *
 *************************************************************************
 *
 * @author Qinlin Jia <qinlinj@andrew.cmu.edu>
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/* Do not change the following! */

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* You can change anything from here onward */

/*
 *****************************************************************************
 * If DEBUG is defined (such as when running mdriver-dbg), these macros      *
 * are enabled. You can use them to print debugging output and to check      *
 * contracts only in debug mode.                                             *
 *                                                                           *
 * Only debugging macros with names beginning "dbg_" are allowed.            *
 * You may not define any other macros having arguments.                     *
 *****************************************************************************
 */
#ifdef DEBUG
/* When DEBUG is defined, these form aliases to useful functions */
#define dbg_requires(expr) assert(expr)
#define dbg_assert(expr) assert(expr)
#define dbg_ensures(expr) assert(expr)
#define dbg_printf(...) ((void)printf(__VA_ARGS__))
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When DEBUG is not defined, these should emit no code whatsoever,
 * not even from evaluation of argument expressions.  However,
 * argument expressions should still be syntax-checked and should
 * count as uses of any variables involved.  This used to use a
 * straightforward hack involving sizeof(), but that can sometimes
 * provoke warnings about misuse of sizeof().  I _hope_ that this
 * newer, less straightforward hack will be more robust.
 * Hat tip to Stack Overflow poster chqrlie (see
 * https://stackoverflow.com/questions/72647780).
 */
#define dbg_discard_expr_(...) ((void)((0) && printf(__VA_ARGS__)))
#define dbg_requires(expr) dbg_discard_expr_("%d", !(expr))
#define dbg_assert(expr) dbg_discard_expr_("%d", !(expr))
#define dbg_ensures(expr) dbg_discard_expr_("%d", !(expr))
#define dbg_printf(...) dbg_discard_expr_(__VA_ARGS__)
#define dbg_printheap(...) ((void)((0) && print_heap(__VA_ARGS__)))
#endif

#define MAX_SEG_LIST_LENGTH 14

/* Basic constants */

typedef uint64_t word_t;

/** @brief Word and header size (bytes) */
static const size_t wsize = sizeof(word_t);

/** @brief Double word size (bytes) */
static const size_t dsize = 2 * wsize;

/** @brief Minimum block size (bytes) */
static const size_t min_block_size = dsize;

static const word_t pre_alloc_mark = 0x2;

static const word_t pre_min_mark = 0x4;
/**
 * TODO: explain what chunksize is
 * (Must be divisible by dsize)
 */
static const size_t chunksize = (1 << 12);

/**
 * TODO: explain what alloc_mask is
 */
static const word_t alloc_mask = 0x1;

/**
 * TODO: explain what size_mask is
 */
static const word_t size_mask = ~(word_t)0xF;

/** @brief Represents the header and payload of one block in the heap */
typedef struct block {
    /** @brief Header contains size + allocation flag */
    word_t header;

    /**
     * @brief A pointer to the block payload.
     *
     * TODO: feel free to delete this comment once you've read it carefully.
     * We don't know what the size of the payload will be, so we will declare
     * it as a zero-length array, which is a GNU compiler extension. This will
     * allow us to obtain a pointer to the start of the payload. (The similar
     * standard-C feature of "flexible array members" won't work here because
     * those are not allowed to be members of a union.)
     *
     * WARNING: A zero-length array must be the last element in a struct, so
     * there should not be any struct fields after it. For this lab, we will
     * allow you to include a zero-length array in a union, as long as the
     * union is the last field in its containing struct. However, this is
     * compiler-specific behavior and should be avoided in general.
     *
     * WARNING: DO NOT cast this pointer to/from other types! Instead, you
     * should use a union to alias this zero-length array with another struct,
     * in order to store additional types of data in the payload memory.
     */
    union {
        struct {
            struct block *next;
            struct block *prev;
        } free_list;
        char payload[0];
    } data;
    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Why can't we declare the block footer here as part of the struct?
     * The block footer cannot be part of the struct due to its variable
     * position, dependent on the runtime-determined block size. Why do we even
     * have footers -- will the code work fine without them? Footers are
     * essential for determining block sizes during coalescing and navigating
     * backward through the heap. which functions actually use the data
     * contained in footers? coalesce_block(), find_prev(), write_footer()
     */
} block_t;

/* Global variables */

/** @brief Pointer to first block in the heap */
static block_t *heap_start = NULL;

/**
 * @brief An array that keeps pointers to each free list
 */
static block_t *seg_list[MAX_SEG_LIST_LENGTH];

/*
 * ---------------------------------------------------------------------------
 *                        BEGIN SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Returns the maximum of two integers.
 * @param[in] x
 * @param[in] y
 * @return `x` if `x > y`, and `y` otherwise.
 */
static size_t max(size_t x, size_t y) {
    return (x > y) ? x : y;
}

/**
 * @brief Rounds `size` up to next multiple of n
 * @param[in] size
 * @param[in] n
 * @return The size after rounding up
 */
static size_t round_up(size_t size, size_t n) {
    return n * ((size + (n - 1)) / n);
}

/**
 * @brief Packs the `size` and `alloc` of a block into a word suitable for
 *        use as a packed value.
 *
 * Packed values are used for both headers and footers.
 *
 * The allocation status is packed into the lowest bit of the word.
 *
 * @param[in] size The size of the block being represented
 * @param[in] alloc True if the block is allocated
 * @return The packed value
 */
static word_t pack(size_t size, bool pre_min, bool pre_alloc, bool alloc) {
    word_t word = size;
    if (alloc) {
        word |= alloc_mask;
    }

    // Set the previous allocated flag if the previous block is allocated.
    if (pre_alloc) {
        word |= pre_alloc_mark;
    }

    // Set the previous minimum flag if the previous block is of minimum size.
    if (pre_min) {
        word |= pre_min_mark;
    }

    return word;
}

/**
 * @brief Extracts the size represented in a packed word.
 *
 * This function simply clears the lowest 4 bits of the word, as the heap
 * is 16-byte aligned.
 *
 * @param[in] word
 * @return The size of the block represented by the word
 */
static size_t extract_size(word_t word) {
    return (word & size_mask);
}

/**
 * @brief Extracts the size of a block from its header.
 * @param[in] block
 * @return The size of the block
 */
static size_t get_size(block_t *block) {
    return extract_size(block->header);
}

/**
 * @brief Given a payload pointer, returns a pointer to the corresponding
 *        block.
 * It calculates the start of the `block_t` structure
 * based on the given payload pointer by subtracting the offset of the payload
 * within the `block_t` structure.
 * @param[in] bp A pointer to a block's payload
 * @return The corresponding block's header.
 */
static block_t *payload_to_header(void *bp) {
    return (block_t *)((char *)bp - offsetof(block_t, data.payload));
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        payload.
 * This function takes a pointer to a block (typically represented by its
 * header) and returns a pointer to the payload of that block. The payload is
 * where user-allocated data is stored. The payload's location is calculated
 * relative to the start of the block.
 * @param[in] block A pointer to the block from which to get the payload.
 * @return A pointer to the block's payload
 * @pre The block must be a valid block, not a boundary tag.
 */
static void *header_to_payload(block_t *block) {
    dbg_requires(get_size(block) != 0);
    // Return a pointer to the payload part of the block.
    return (void *)(block->data.payload);
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        footer.
 * @param[in] block A pointer to the block from which to get the footer.
 * @return A pointer to the block's footer
 * @pre The block must be a valid block, not a boundary tag.
 */
static word_t *header_to_footer(block_t *block) {
    dbg_requires(get_size(block) != 0 &&
                 "Called header_to_footer on the epilogue block");
    return (word_t *)(block->data.payload + get_size(block) - dsize);
}

/**
 * @brief Given a block footer, returns a pointer to the corresponding
 *        header.
 * @param[in] footer A pointer to the block's footer
 * @return A pointer to the start of the block
 * @pre The footer must be the footer of a valid block, not a boundary tag.
 */
static block_t *footer_to_header(word_t *footer) {
    size_t size = extract_size(*footer);
    dbg_assert(size != 0 && "Called footer_to_header on the prologue block");
    return (
        block_t *)((char *)footer + wsize -
                   size); // Subtract only the header size, not the footer size.
}

/**
 * @brief Returns the payload size of a given block.
 *
 * The payload size is equal to the entire block size minus the sizes of the
 * block's header and footer.
 *
 * @param[in] block
 * @return The size of the block's payload
 */
static size_t get_payload_size(block_t *block) {
    size_t asize = get_size(block);
    return asize - wsize;
}

/**
 * @brief Returns the allocation status of a given header value.
 *
 * This is based on the lowest bit of the header value.
 *
 * @param[in] word
 * @return The allocation status correpsonding to the word
 */
static bool extract_alloc(word_t word) {
    return (bool)(word & alloc_mask);
}

/**
 * @brief Returns the allocation status of a block, based on its header.
 * @param[in] block
 * @return The allocation status of the block
 */
static bool get_alloc(block_t *block) {
    return extract_alloc(block->header);
}

/**
 * @brief Writes an epilogue header at the given address.
 *
 * The epilogue block serves as a marker at the end of the heap. It is written
 * with a size of 0 and an allocation status of true (allocated). This function
 * does not set any flags for previous block allocation or minimum size, as the
 * epilogue is just a boundary marker and not a real block.
 *
 * @param[out] block The location to write the epilogue header
 */
static void write_epilogue(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires((char *)block == (char *)mem_heap_hi() - 7);
    block->header =
        pack(0, false, false, true); // Size is 0, block is marked as allocated.
}

// /**
//  * @brief Writes a block starting at the given address.
//  *
//  * This function writes both a header and footer, where the location of the
//  * footer is computed in relation to the header.
//  *
//  * TODO: Are there any preconditions or postconditions?
//  *
//  * @param[out] block The location to begin writing the block header
//  * @param[in] size The size of the new block
//  * @param[in] alloc The allocation status of the new block
//  */
// static void write_block(block_t *block, size_t size, bool alloc) {
//     dbg_requires(block != NULL);
//     dbg_requires(size > 0);
//     block->header = pack(size, alloc);
//     word_t *footerp = header_to_footer(block);
//     *footerp = pack(size, alloc);
// }

/**
 * @brief Finds the next consecutive block on the heap.
 *
 * This function accesses the next block in the "implicit list" of the heap
 * by adding the size of the block.
 *
 * @param[in] block A block in the heap
 * @return The next consecutive block on the heap
 * @pre The block is not the epilogue
 */
static block_t *find_next(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(get_size(block) != 0 &&
                 "Called find_next on the last block in the heap");
    return (block_t *)((char *)block + get_size(block));
}

/**
 * @brief Finds the footer of the previous block on the heap.
 * @param[in] block A block in the heap
 * @return The location of the previous block's footer
 */
static word_t *find_prev_footer(block_t *block) {
    // Compute previous footer position as one word before the header
    return &(block->header) - 1;
}

/**
 * @brief Finds the previous consecutive block on the heap.
 *
 * This is the previous block in the "implicit list" of the heap.
 *
 * If the function is called on the first block in the heap, NULL will be
 * returned, since the first block in the heap has no previous block!
 *
 * The position of the previous block is found by reading the previous
 * block's footer to determine its size, then calculating the start of the
 * previous block based on its size.
 *
 * @param[in] block A block in the heap
 * @return The previous consecutive block in the heap.
 */
static block_t *find_prev(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(get_size(block) != 0);
    word_t *footerp = find_prev_footer(block);
    return footer_to_header(footerp);
}

/**
 * @brief Determines the appropriate segregated list class for a block based on
 * its size.
 *
 * This function finds the right class in the segregated free list for a block
 * of a given size. The classes are determined based on size thresholds, which
 * double at each step. The function returns an index (starting from 0)
 * indicating the appropriate class.
 *
 * @param[in] size The size of the block for which to find the segregated list
 * class.
 * @return The index of the segregated list class appropriate for the given
 * block size.
 *
 */
static int find_seg_list_class(size_t size) {
    if (size <= min_block_size) {
        return 0;
    }

    int seg_list_class = 1;
    while (size > (min_block_size << seg_list_class)) {
        seg_list_class++;
    }

    // Ensure the class doesn't exceed the maximum
    return (seg_list_class < MAX_SEG_LIST_LENGTH) ? seg_list_class
                                                  : (MAX_SEG_LIST_LENGTH - 1);
}

/**
 * @brief Extracts the pre-allocation status from a block's header.
 *
 * This function checks the pre-allocation status of a block by examining the
 * appropriate bit in its header. The pre-allocation status indicates whether
 * the block immediately before the given block is allocated or not.
 *
 */
static bool get_pre_alloc(block_t *block) {
    dbg_requires(block != NULL);
    return (bool)(block->header & pre_alloc_mark);
}

/**
 * @brief Extracts the pre-minimum status from a block's header.
 *
 * This function checks the pre-minimum status of a block by examining the
 * appropriate bit in its header. The pre-minimum status indicates whether
 * the block has a size equal to the minimum block size or not.
 *
 * @param[in] block The block from which to extract the pre-minimum status.
 * @return True if the block has a size equal to the minimum block size, False
 * otherwise.
 */
static bool get_pre_min(block_t *block) {
    dbg_requires(block != NULL);
    word_t word = block->header;
    return (bool)(word & pre_min_mark);
}

static void write_block(block_t *block, size_t size, bool pre_min,
                        bool pre_alloc, bool alloc, bool write_footer) {
    dbg_requires(block != NULL);

    block->header = pack(size, pre_min, pre_alloc, alloc);
    if (write_footer && (size != min_block_size)) {
        word_t *footerp = header_to_footer(block);
        *footerp = pack(size, pre_min, pre_alloc, alloc);
    }
}

/**
 * @brief Sets the allocation and minimization status for the header of the next
 * block.
 *
 * This function updates the next block's header with the specified 'pre_min'
 * and 'pre_alloc' flags. It is used to maintain the heap consistency after
 * operations like block allocation or free.
 *
 * @param[in] block The current block.
 * @param[in] next_pre_min The 'pre_min' flag for the next block.
 * @param[in] next_pre_alloc The 'pre_alloc' flag for the next block.
 */
static void set_next_block_pre_alloc_pre_min(block_t *block, bool next_pre_min,
                                             bool next_pre_alloc) {
    block_t *block_next = find_next(block);
    write_block(block_next, get_size(block_next), next_pre_min, next_pre_alloc,
                get_alloc(block_next), false);
}

/**
 * @brief Finds the previous block in the heap when the 'pre_min' flag is set.
 *
 * This function calculates the address of the previous block based on the
 * 'pre_min' flag indicating a minimally-sized previous block. It's used in heap
 * coalescing and block management.
 *
 * @param[in] block The current block.
 * @return The previous block in the heap.
 */
static block_t *find_min_prev(block_t *block) {
    dbg_requires(get_pre_min(block));
    // Calculate the previous block's address based on the minimally-sized block
    // property
    return (block_t *)((char *)block - dsize);
}
/*
 * ---------------------------------------------------------------------------
 *                        END SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/******** The remaining content below are helper and debug routines ********/

/**
 * @brief Inserts a new free block into the segregated free list using LIFO
 * policy.
 *
 * This function adds a new free block at the beginning of the appropriate
 * segregated list, based on its size. It handles two cases:
 * 1. For minimum size blocks, it simply inserts the block at the start.
 * 2. For other sizes, it also adjusts the previous pointer of the next block in
 * the list.
 *
 * @param[in] block The new free block to be inserted into the free list.
 */
static void insert_block_LIFO(block_t *block) {
    size_t size = get_size(block);
    int class = find_seg_list_class(size);

    // Handle the insertion for both minimum and non-minimum size blocks
    block_t *first_block = seg_list[class];

    // Set the previous pointer of the existing first block if it's not a
    // minimum size block
    if (size != min_block_size && first_block != NULL) {
        first_block->data.free_list.prev = block;
    }

    // Update the block's next pointer to point to the current first block
    block->data.free_list.next = first_block;

    // For non-minimum size blocks, reset the previous pointer
    if (size != min_block_size) {
        block->data.free_list.prev = NULL;
    }

    // Update the segregated list to point to the new first block
    seg_list[class] = block;
}
/**
 * @brief Coalesces a given block with adjacent free blocks if possible.
 *
 * This function checks the allocation status of adjacent blocks (previous and
 * next) and coalesces with them if they are free. This helps in reducing
 * external fragmentation. There are four cases based on the allocation status
 * of adjacent blocks:
 * 1. Both adjacent blocks are allocated: No coalescing is done.
 * 2. Only next block is free: Coalesce with next block.
 * 3. Only previous block is free: Coalesce with previous block.
 * 4. Both adjacent blocks are free: Coalesce with both.
 * After coalescing, the block is inserted into the free list.
 *
 * @param[in] block The current block to coalesce.
 * @return The new coalesced block, or the original block if no coalescing
 * occurred.
 */
static void fix_free_list(block_t *block) {
    dbg_requires(block != NULL);

    size_t size = get_size(block);
    int class = find_seg_list_class(size);

    if (size != min_block_size) {
        // Handle non-minimum size blocks
        block_t *prev = block->data.free_list.prev;
        block_t *nextv = block->data.free_list.next;

        if (prev == NULL) {
            // Block is the first in the list
            if (nextv != NULL) {
                nextv->data.free_list.prev = NULL;
            }
            seg_list[class] = nextv;
        } else {
            // Block is in the middle or end of the list
            if (nextv != NULL) {
                nextv->data.free_list.prev = prev;
            }
            prev->data.free_list.next = nextv;
        }

        // Reset the block's next and previous pointers
        block->data.free_list.next = NULL;
        block->data.free_list.prev = NULL;
    } else {
        // Handle minimum size blocks
        block_t *nextv = block->data.free_list.next;
        block_t *prev = NULL;
        block_t *temp = seg_list[class];
        dbg_requires(seg_list[class] != NULL);

        // Iterate to find the block in the list
        while (temp != NULL) {
            if (temp->data.free_list.next == block) {
                prev = temp;
                break;
            }
            temp = temp->data.free_list.next;
        }

        // Remove the block from the list
        if (prev != NULL) {
            prev->data.free_list.next = nextv;
        } else {
            seg_list[class] = nextv;
        }
    }
}

static void handle_case_1(block_t *block, size_t size);
static void handle_case_2(block_t *block, block_t *next_block, size_t size,
                          bool pre_min, bool pre_flag);
static void handle_case_3(block_t *pre_block, size_t size, bool pre_min);
static void handle_case_4(block_t *pre_block, block_t *next_block, size_t size,
                          bool pre_min);

/**
 * @brief Coalesces a given block with adjacent free blocks if possible.
 *
 * This function checks the allocation status of adjacent blocks (previous and
 * next) and coalesces with them if they are free. This helps in reducing
 * external fragmentation. There are four cases based on the allocation status
 * of adjacent blocks:
 * 1. Both adjacent blocks are allocated: No coalescing is done.
 * 2. Only next block is free: Coalesce with next block.
 * 3. Only previous block is free: Coalesce with previous block.
 * 4. Both adjacent blocks are free: Coalesce with both.
 * After coalescing, the block is inserted into the free list.
 *
 * @param[in] block The current block to coalesce.
 * @return The new coalesced block, or the original block if no coalescing
 * occurred.
 *
 * Preconditions:
 * - The input block must be free (not allocated).
 * - The block must be a valid block within the heap boundaries.
 *
 * Postconditions:
 * - The block is coalesced with adjacent free blocks if possible.
 * - The returned block is inserted into the free list.
 */
static block_t *coalesce_block(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(!get_alloc(block));

    // Determine the allocation status of adjacent blocks
    bool pre_flag = get_pre_alloc(block);
    bool next_flag = get_alloc(find_next(block));
    size_t size = get_size(block);

    // Find the next and potential previous block
    block_t *next_block = find_next(block);
    block_t *pre_block = NULL;

    // Find the previous block if it's free
    if (!pre_flag) {
        pre_block =
            get_pre_min(block) ? find_min_prev(block) : find_prev(block);
    }

    // Coalesce the block based on the allocation status of adjacent blocks
    if (pre_flag && next_flag) {
        handle_case_1(block, size);
    } else if (pre_flag && !next_flag) {
        handle_case_2(block, next_block, size, get_pre_min(block), pre_flag);
    } else if (!pre_flag && next_flag) {
        dbg_requires(pre_block != block);
        handle_case_3(pre_block, size, get_pre_min(pre_block));
        block =
            pre_block; // Update block pointer if coalesced with previous block
    } else if (!pre_flag && !next_flag) {
        dbg_requires(pre_block != block);
        handle_case_4(pre_block, next_block, size, get_pre_min(pre_block));
        block = pre_block; // Update block pointer if coalesced with both blocks
    }

    // Insert the coalesced block into the free list
    insert_block_LIFO(block);

    return block;
}

// Case 1: Both front and back blocks have been assigned
static void handle_case_1(block_t *block, size_t size) {
    if (size == min_block_size) {
        set_next_block_pre_alloc_pre_min(block, true, false);
    } else if (size > min_block_size) {
        set_next_block_pre_alloc_pre_min(block, false, false);
    }
}

// Case 2: Front block allocated, back block unallocated
static void handle_case_2(block_t *block, block_t *next_block, size_t size,
                          bool pre_min, bool pre_flag) {
    fix_free_list(next_block);
    size += get_size(next_block);
    write_block(block, size, pre_min, pre_flag, false, true);
    set_next_block_pre_alloc_pre_min(block, false, false);
}

// Case 3: Front block unallocated, back block allocated
static void handle_case_3(block_t *pre_block, size_t size, bool pre_min) {
    fix_free_list(pre_block);
    size += get_size(pre_block);
    write_block(pre_block, size, pre_min, true, false, true);
    set_next_block_pre_alloc_pre_min(pre_block, false, false);
}

// Case 4: Both front and back blocks unallocated
static void handle_case_4(block_t *pre_block, block_t *next_block, size_t size,
                          bool pre_min) {
    fix_free_list(pre_block);
    fix_free_list(next_block);
    size += get_size(pre_block) + get_size(next_block);
    write_block(pre_block, size, pre_min, true, false, true);
    set_next_block_pre_alloc_pre_min(pre_block, false, false);
}

/**
 * @brief Extends the heap with a new free block.
 *
 * This function increases the heap size by a specified amount and initializes
 * a new free block in the extended space. If the previous block is free, it
 * attempts to coalesce this new block with the previous one. The function also
 * handles the creation of a new epilogue block at the end of the heap.
 *
 * @param[in] size The size by which the heap should be extended.
 * @return Pointer to the new block, or NULL if the extension fails.
 *
 * Preconditions:
 * - The size should be non-zero and sufficient to cover the block overhead.
 * - The memory system (mem_sbrk) should be able to accommodate the requested
 * increase.
 *
 * Postconditions:
 * - The heap is extended by at least the requested size.
 * - Returns a new free block of at least the requested size.
 * - Returns NULL if the heap cannot be extended.
 */
static block_t *extend_heap(size_t size) {
    // Align size to meet memory alignment requirements
    size = round_up(size, dsize);

    // Extend the heap by the aligned size and check for errors
    void *bp = mem_sbrk((intptr_t)size);
    if (bp == (void *)(-1)) {
        return NULL;
    }

    // Convert the pointer to the new space to a block pointer
    block_t *block = payload_to_header(bp);

    // Retrieve the allocation status of the previous block
    bool pre_min = get_pre_min(block);
    bool pre_alloc = get_pre_alloc(block);

    // Initialize the new block as a free block
    write_block(block, size, pre_min, pre_alloc, false, true);

    // Create a new epilogue block at the end of the heap
    block_t *block_next = find_next(block);
    write_epilogue(block_next);

    // Coalesce the new block with the previous block if it's free
    block = coalesce_block(block);

    return block;
}

/**
 * @brief Splits a given free block if it is large enough.
 *
 * This function splits a free block into two parts if the block is larger than
 * the required size plus the minimum block size. The first part will be of the
 * requested size (asize), and the second part will remain as a free block. If
 * the block is not large enough to split, it marks the entire block as
 * allocated.
 *
 * @param[in] block The block to be split.
 * @param[in] asize The size required for the allocation.
 *
 * Preconditions:
 * - The input block must be free (not allocated).
 * - The size of the block must be at least as large as the requested size.
 *
 * Postconditions:
 * - If split occurs, the original block is reduced to the requested size and
 * marked as allocated.
 * - Any remaining part of the block becomes a separate free block.
 * - If no split occurs, the entire block is marked as allocated.
 */
static void split_block(block_t *block, size_t asize) {
    // Ensure the block is free and of sufficient size
    dbg_requires(!get_alloc(block));
    size_t block_size = get_size(block);
    dbg_requires(block_size >= asize);

    // Remove the block from the free list
    fix_free_list(block);

    // Check if the block can be split
    if ((block_size - asize) >= min_block_size) {
        // Initialize the next block
        block_t *block_next;

        // Write the header for the allocated part of the split block
        bool pre_min = get_pre_min(block);
        write_block(block, asize, pre_min, true, true, false);

        // Calculate and set the header and footer for the remaining free part
        bool next_pre_min = asize == min_block_size;
        block_next = find_next(block);
        write_block(block_next, block_size - asize, next_pre_min, true, false,
                    true);

        // Update the pre_alloc status for the block following the newly free
        // block
        bool next_next_pre_min = (block_size - asize) == min_block_size;
        set_next_block_pre_alloc_pre_min(block_next, next_next_pre_min, false);

        // Attempt to coalesce the remaining free part with adjacent free blocks
        coalesce_block(block_next);
    } else {
        // If the block can't be split, mark the entire block as allocated
        bool pre_min = get_pre_min(block);
        write_block(block, block_size, pre_min, true, true, false);

        // Update the pre_alloc status of the next block
        bool next_pre_min = get_pre_min(find_next(block));
        set_next_block_pre_alloc_pre_min(block, next_pre_min, true);
    }

    // Ensure the block is now allocated
    dbg_ensures(get_alloc(block));
}

/**
 * @brief Finds a free block of memory that fits the requested size.
 *
 * This function iterates through segregated free lists to find a free block
 * that is large enough to fit the requested size. It starts searching from
 * the list that best matches the requested size and moves to higher lists
 * if necessary.
 *
 * @param[in] asize The size of the memory block needed.
 * @return Pointer to a suitable free block if found, otherwise NULL.
 *
 * Preconditions:
 * - The segregated free lists must be properly initialized.
 * - The size 'asize' should be aligned and include the overhead for the block
 * header/footer.
 *
 * Postconditions:
 * - Returns a block that is large enough to fit the requested size.
 * - Returns NULL if no suitable block is found.
 */
static block_t *find_fit(size_t asize) {
    // Start searching from the segregated list class that best fits the
    // requested size
    int class = find_seg_list_class(asize);

    while (class < MAX_SEG_LIST_LENGTH) {
        block_t *class_root = seg_list[class];

        // Iterate through the blocks in the current segregated list
        while (class_root != NULL) {
            size_t size = get_size(class_root);

            // If a block is found that is large enough, return it
            if (size >= asize) {
                return class_root;
            }

            // Move to the next block in the current segregated list
            class_root = class_root->data.free_list.next;
        }

        // If no suitable block was found in the current list, move to the next
        // higher list
        class ++;
    }

    // Return NULL if no suitable block was found in any of the lists
    return NULL;
}

bool check_header_footer(block_t *block);
bool check_free_list(void);
bool check_free_block_counts(void);

/**
 * @brief Check the consistency of the heap at the given line.
 *
 * This function checks various aspects of the heap for consistency, including
 * prologue and epilogue blocks, block alignments, header-footer matches, and
 * the consistency of free blocks.
 *
 * @param[in] line - The line number from which this function is called.
 * @return true if the heap is consistent, false otherwise.
 */
bool mm_checkheap(int line) {
    // Check prologue footer
    word_t prologue_footer = *(find_prev_footer(heap_start));
    if (extract_size(prologue_footer) != 0 ||
        extract_alloc(prologue_footer) != 1) {
        printf("Error at line %d: Bad prologue footer\n", line);
        return false;
    }

    block_t *block = heap_start;
    bool pre_alloc_flag = true; // Tracks if the previous block was allocated

    // Iterate through blocks in the heap
    for (; get_size(block) > 0; block = find_next(block)) {
        size_t size = get_size(block);

        // Check for doubleword alignment and header-footer match for
        // non-minimum blocks
        if ((size % dsize) != 0 ||
            (size != min_block_size && !check_header_footer(block))) {
            printf("Error at line %d: Block at %p has alignment or "
                   "header-footer mismatch\n",
                   line, (void *)block);
            return false;
        }

        // Check for consecutive free blocks
        if (!get_alloc(block) && !pre_alloc_flag) {
            printf("Error at line %d: Consecutive free blocks found\n", line);
            return false;
        }
        pre_alloc_flag = get_alloc(block);
    }

    // Check epilogue header
    if (extract_size(block->header) != 0 || extract_alloc(block->header) != 1) {
        printf("Error at line %d: Bad epilogue header\n", line);
        return false;
    }

    // Check free list pointers and bucket size consistency
    if (!check_free_list()) {
        printf("Error at line %d: Free list pointer or bucket size "
               "inconsistency\n",
               line);
        return false;
    }

    // Check consistency of free block counts
    if (!check_free_block_counts()) {
        printf("Error at line %d: Mismatch in free block counts\n", line);
        return false;
    }

    return true;
}

/**
 * @brief Check the consistency of the header and footer of a block.
 *
 * This function checks if the header and footer of a block have consistent
 * values for size and allocation status.
 *
 * @param[in] block - Pointer to the block to check.
 * @return true if the header and footer are consistent, false otherwise.
 */
bool check_header_footer(block_t *block) {
    // Extract the size from the footer
    size_t footer_size = extract_size(*(header_to_footer(block)));

    // Check if the size and allocation status of the block match the footer
    return get_size(block) == footer_size &&
           get_alloc(block) == extract_alloc(*(header_to_footer(block)));
}

/**
 * @brief Check the consistency of the free list.
 *
 * This function iterates through the free list and checks the pointers and
 * size consistency of the free blocks. It also ensures that the pointers are
 * within the bounds of the heap.
 *
 * @return true if the free list is consistent, false otherwise.
 */
bool check_free_list(void) {
    // Iterate through the segmentation list and check pointers and size
    // consistency
    for (int class = 0; class < MAX_SEG_LIST_LENGTH; class ++) {
        block_t *temp = seg_list[class];
        while (temp != NULL) {
            // Check if pointers are within heap bounds
            if (temp->data.free_list.next > (block_t *)mem_heap_hi() ||
                temp->data.free_list.next < (block_t *)mem_heap_lo()) {
                printf("Error: Free list pointer out of heap bounds\n");
                return false;
            }
            // Further checks for consistency can be added here
            temp = temp->data.free_list.next;
        }
    }
    return true;
}

/**
 * @brief Check if the counts of free blocks in the heap match the free list.
 *
 * This function compares the counts of free blocks in the heap with the counts
 * in the free list to ensure they match.
 *
 * @return true if the counts match, false otherwise.
 */
bool check_free_block_counts(void) {
    int free_block_count = 0;
    for (block_t *block = heap_start; get_size(block) > 0;
         block = find_next(block)) {
        if (!get_alloc(block)) {
            free_block_count++;
        }
    }

    int free_list_count = 0;
    for (int class = 0; class < MAX_SEG_LIST_LENGTH; class ++) {
        for (block_t *temp = seg_list[class]; temp != NULL;
             temp = temp->data.free_list.next) {
            free_list_count++;
        }
    }

    // Compare the counts of free blocks in the heap and free list
    if (free_block_count != free_list_count) {
        printf("Error: Free block count does not match free list count\n");
        return false;
    }

    return true;
}

/**
 * @brief Initialize the memory allocator.
 *
 * This function initializes the memory allocator by creating the initial empty
 * heap, setting up the heap prologue and epilogue, and extending the empty heap
 * with a free block of `chunksize` bytes. It also reinitializes the
 * segmentation list for free blocks.
 *
 * @return true if initialization is successful, false otherwise.
 *
 * Postconditions:
 * - The initial empty heap is created.
 * - The heap prologue and epilogue are set up.
 * - The segmentation list for free blocks is reinitialized.
 * - The empty heap is extended with a free block of at least `chunksize` bytes.
 * - If initialization fails, false is returned.
 */
bool mm_init(void) {
    // printf("start");
    // Create the initial empty heap
    // printf("start\n");
    word_t *start = (word_t *)(mem_sbrk(2 * wsize));

    if (start == (void *)-1) {
        return false;
    }

    start[0] = pack(0, false, true, true); // Heap prologue (block footer)
    start[1] = pack(0, false, true, true); // Heap epilogue (block header)

    // Heap starts with first "block header", currently the epilogue
    heap_start = (block_t *)&(start[1]);
    // reinitialize seg list
    for (int index = 0; index < MAX_SEG_LIST_LENGTH; index++) {
        seg_list[index] = NULL;
    }

    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(chunksize) == NULL) {
        return false;
    }

    return true;
}

/**
 * @brief Allocate a block of memory with the requested size.
 *
 * This function allocates a block of memory with the requested size and returns
 * a pointer to the allocated memory. It adjusts the block size to meet
 * alignment requirements and searches the free list for a suitable block. If no
 * suitable block is found in the free list, it extends the heap to allocate
 * more memory.
 *
 * @param[in] size Requested size of the memory block to allocate.
 * @return A pointer to the allocated memory block, or NULL if allocation fails.
 *
 * Preconditions:
 * - The heap should be initialized by calling `mm_init` if it hasn't been done
 * yet.
 * - The requested size should be greater than 0.
 *
 * Postconditions:
 * - A block of memory is allocated, and its header is marked as allocated.
 * - Searches for a suitable block in the free list.
 * - If no fit is found in the free list, it extends the heap by at least
 * `chunksize`.
 * - The block is split if it's larger than the requested size, and the
 * remaining portion is marked as free.
 */
void *malloc(size_t size) {
    dbg_requires(mm_checkheap(__LINE__));

    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    // Initialize heap if it isn't initialized
    if (heap_start == NULL) {
        if (!(mm_init())) {
            dbg_printf("Problem initializing heap. Likely due to sbrk");
            return NULL;
        }
    }

    // Ignore spurious request
    if (size == 0) {
        dbg_ensures(mm_checkheap(__LINE__));
        return bp;
    }

    // Adjust block size to include overhead and to meet alignment requirements
    asize = round_up(size + wsize, dsize);

    // Search the free list for a fit
    block = find_fit(asize);

    // If no fit is found, request more memory, and then and place the block
    if (block == NULL) {
        // Always request at least chunksize
        extendsize = max(asize, chunksize);
        block = extend_heap(extendsize);
        // extend_heap returns an error
        if (block == NULL) {
            return bp;
        }
    }

    // The block should be marked as free
    dbg_assert(!get_alloc(block));

    split_block(block, asize);

    bp = header_to_payload(block);

    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
}

/**
 * @brief Deallocate a block of memory and perform coalescing if possible.
 *
 * This function deallocates a block of memory pointed to by `bp`. It updates
 * the header and footer of the block to mark it as free. If possible, it also
 * coalesces the freed block with its neighboring free blocks.
 *
 * @param[in] bp Pointer to the block of memory to be deallocated.
 */
void free(void *bp) {
    dbg_requires(mm_checkheap(__LINE__));

    if (bp == NULL) {
        return;
    }

    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);

    // The block should be marked as allocated
    dbg_assert(get_alloc(block));

    // Mark the block as free
    // Mark the block as free by updating its header (and footer if necessary)
    bool pre_alloc = get_pre_alloc(block);
    bool pre_min = get_pre_min(block);
    write_block(block, size, pre_min, pre_alloc, false, size != min_block_size);

    // Update the 'pre_alloc' status of the next block in the heap
    // If the block size is not minimum, mark the next block as not
    // pre-allocated
    if (size != min_block_size) {
        set_next_block_pre_alloc_pre_min(block, false, false);
    } else {
        // If the block is of minimum size, mark the next block as pre-allocated
        set_next_block_pre_alloc_pre_min(block, true, false);
    }

    // Try to coalesce the block with its neighbors
    coalesce_block(block);

    dbg_ensures(mm_checkheap(__LINE__));
}

/**
 * @brief
 *
 * This function reallocates a block of memory pointed to by `ptr` to a new size
 * specified by `size`. It returns a pointer to the reallocated memory block.
 *
 * @param[in] ptr  Pointer to the previously allocated memory block to be
 * reallocated.
 * @param[in] size New size in bytes for the reallocated block.
 * @return        Pointer to the reallocated memory block, or NULL if
 * reallocation fails.
 *
 * @param[in] ptr
 * @param[in] size
 * @return
 */
void *realloc(void *ptr, size_t size) {
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL) {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);

    // If malloc fails, the original block is left untouched
    if (newptr == NULL) {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if (size < copysize) {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

    dbg_ensures(mm_checkheap(__LINE__));

    return newptr;
}

/**
 * @brief Allocate and initialize memory for an array of elements.
 *
 * This function allocates a block of memory for an array of `elements` each
 * `size` bytes long. It returns a pointer to the allocated memory block,
 * which is initialized with all bits set to 0.
 *
 * If `elements` or `size` is 0, it returns NULL.
 * If the multiplication of `elements` and `size` would cause an overflow, it
 * returns NULL.
 *
 * @param[in] elements Number of elements in the array.
 * @param[in] size     Size in bytes of each element.
 * @return            Pointer to the allocated memory block, or NULL if
 * allocation fails.
 */
void *calloc(size_t elements, size_t size) {
    void *bp;
    size_t asize = elements * size;

    if (elements == 0) {
        return NULL;
    }
    if (asize / elements != size) {
        // Multiplication overflowed
        return NULL;
    }

    bp = malloc(asize);
    if (bp == NULL) {
        return NULL;
    }

    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}

/*
 *****************************************************************************
 * Do not delete the following super-secret(tm) lines!                       *
 *                                                                           *
 * 53 6f 20 79 6f 75 27 72 65 20 74 72 79 69 6e 67 20 74 6f 20               *
 *                                                                           *
 * 66 69 67 75 72 65 20 6f 75 74 20 77 68 61 74 20 74 68 65 20               *
 * 68 65 78 61 64 65 63 69 6d 61 6c 20 64 69 67 69 74 73 20 64               *
 * 6f 2e 2e 2e 20 68 61 68 61 68 61 21 20 41 53 43 49 49 20 69               *
 *                                                                           *
 * 73 6e 27 74 20 74 68 65 20 72 69 67 68 74 20 65 6e 63 6f 64               *
 * 69 6e 67 21 20 4e 69 63 65 20 74 72 79 2c 20 74 68 6f 75 67               *
 * 68 21 20 2d 44 72 2e 20 45 76 69 6c 0a c5 7c fc 80 6e 57 0a               *
 *                                                                           *
 *****************************************************************************
 */
