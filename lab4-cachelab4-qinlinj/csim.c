/*
 * Author: Qinlin Jia
 * Andrew ID: qinlinj
 * Date: 2023-10-12
 * This is a C program that simulates a cache.
 */

#include "cachelab.h"
#include "getopt.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINELEN 128 // Maximum length for each line read from the trace file.

// This structure defines a single line in a cache set, detailing its metadata
// and data.
typedef struct {
    unsigned long valid; // Valid bit to check if the block contains valid data.
    unsigned long tag;   // The tag extracted from the address.
    unsigned long
        lru; // Counter to implement the Least Recently Used (LRU) policy.
    unsigned long dirty; // Indicates if this block has been modified but not
                         // written back to memory.
} cache_line;

// A set in a cache is essentially an array of cache lines.
typedef cache_line *cache_set;

// A cache consists of multiple cache sets.
typedef cache_set *cache;

// Create a new cache given the s, E, and b parameters.
cache create_cache(long s, long E, long b);

// Deallocate the memory associated with the cache.
void free_cache(cache c, long s, long E);

// Process the given trace file, simulating each memory access against the
// cache.
int process_trace_file(const char *trace, cache cache_sim, csim_stats_t *stats,
                       long s, long E, long b, int verbose);

// Simulate a cache access for the given memory address and update the cache and
// statistics accordingly.
void access_data(cache c, unsigned long address, csim_stats_t *stats, long s,
                 long E, long b, char operation, int verbose);

// Print the help message.
void print_usage(void);

int main(int argc, char **argv) {
    // Command line options and arguments
    int opt; // Option character returned by getopt.
    long s = -1, E = -1,
         b = -1; // Parameters for cache: s = # of set index bits, E = lines per
                 // set, b = block size bits.
    char *t = NULL;                       // Trace file name.
    int verbose = 0;                      // Verbose flag.
    csim_stats_t stats = {0, 0, 0, 0, 0}; // Cache simulation statistics.
    cache cache_sim;                      // The simulated cache.

    // Parse command line arguments using getopt.
    while ((opt = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (opt) {
        case 's': // Number of set index bits.
            s = atoi(optarg);
            break;
        case 'E': // Number of lines per set.
            E = atoi(optarg);
            break;
        case 'b': // Number of block size bits.
            b = atoi(optarg);
            break;
        case 't': // Trace file name.
            t = optarg;
            break;
        case 'v': // Set verbose mode.
            verbose = 1;
            break;
        case 'h': // Display help message.
        default:
            print_usage(); // If -h or invalid argument is provided, print the
                           // usage.
            exit(0);
        }
    }

    // Check if all required command-line arguments have been provided.
    if (s == -1 || E == -1 || b == -1 || t == NULL) {
        fprintf(stderr, "Error: Missing required command line argument\n");
        exit(1);
    }

    // Initialize the cache.
    cache_sim = create_cache(s, E, b);

    // Process each memory access in the trace file.
    process_trace_file(t, cache_sim, &stats, s, E, b, verbose);

    // After processing the trace file, compute the number of dirty bytes in the
    // cache.
    for (int i = 0; i < (1 << s); i++) {
        for (int j = 0; j < E; j++) {
            // If a cache line is valid and dirty, add its size to the dirty
            // bytes count.
            if (cache_sim[i][j].valid && cache_sim[i][j].dirty) {
                stats.dirty_bytes += (1 << b);
            }
        }
    }

    // Cleanup: free the memory used for the cache.
    free_cache(cache_sim, s, E);

    // Display the final cache access statistics.
    printSummary(&stats);

    return 0;
}

// Print the help message.
void print_usage(void) {
    printf("Usage: program_name -s <set_bits> -E <lines> -b <block_bits> -t "
           "<tracefile>\n");
    printf("Options:\n");
    printf("  -h          : Print help message.\n");
    printf("  -s <num>    : Number of set index bits (S = 2^num is the number "
           "of sets).\n");
    printf("  -E <num>    : Number of lines per set.\n");
    printf("  -b <num>    : Number of block offset bits (B = 2^num is the "
           "block size).\n");
    printf("  -t <file>   : Name of the valgrind trace to replay.\n");
    exit(0);
}

/**
 * Create and initialize a cache.
 *
 * @param s The number of set index bits (log2 of the number of sets).
 * @param E The number of lines per set.
 * @param b The number of block size bits (log2 of block size).
 * @return A newly allocated cache structure.
 */
cache create_cache(long s, long E, long b) {
    // Calculate the total number of sets using the formula 2^s.
    unsigned long S = (1UL << s);

    // Allocate memory for the cache sets.
    cache new_cache = (cache_set *)malloc(sizeof(cache_set) * S);

    for (unsigned long i = 0; i < S; i++) {
        // For each set, allocate memory for 'E' cache lines.
        new_cache[i] =
            (cache_line *)malloc(sizeof(cache_line) * (unsigned long)E);

        // Initialize each cache line in the set.
        for (long j = 0; j < E; j++) {
            new_cache[i][j].valid = 0;
            new_cache[i][j].tag = 0;
            new_cache[i][j].lru = 0;
            new_cache[i][j].dirty = 0;
        }
    }

    return new_cache;
}

/**
 * Deallocate memory used by the cache.
 *
 * @param c The cache to be deallocated.
 * @param s The number of set index bits.
 * @param E The number of lines per set.
 */
void free_cache(cache c, long s, long E) {
    // Calculate the total number of sets using the formula 2^s.
    long S = (1 << s);

    // For each set in the cache, free its memory.
    for (long i = 0; i < S; i++) {
        free(c[i]);
    }

    // Finally, free the cache memory.
    free(c);
}

// Process the memory accesses in the trace file and simulate cache behavior.
int process_trace_file(const char *trace, cache cache_sim, csim_stats_t *stats,
                       long s, long E, long b, int verbose) {
    // Open the trace file for reading.
    FILE *tfp = fopen(trace, "r");

    if (!tfp) {
        // If the trace file can't be opened, display an error and exit.
        fprintf(stderr, "Error opening '%s': %s\n", trace, strerror(errno));
        return 1;
    }

    char linebuf[LINELEN];

    // For each line in the trace file...
    while (fgets(linebuf, LINELEN, tfp)) {
        char operation;        // Memory access type: 'L', 'S', or 'M'.
        unsigned long address; // Memory address.
        int size;              // Number of bytes accessed.

        // Parse the line to extract operation, address, and size.
        if (sscanf(linebuf, " %c %lx,%d", &operation, &address, &size) != 3) {
            fprintf(stderr, "Error parsing trace file\n");
            fclose(tfp);
            return 1;
        }

        // Simulate the cache access for the extracted memory address and
        // operation.
        access_data(cache_sim, address, stats, s, E, b, operation, verbose);
    }

    // Close the trace file.
    fclose(tfp);

    return 0;
}

// Simulate a cache access for a given memory address.
void access_data(cache c, unsigned long address, csim_stats_t *stats, long s,
                 long E, long b, char operation, int verbose) {
    // Extract the set index and tag from the given address.
    unsigned long tag =
        address >> (s + b); // Extract the tag bits by discarding the set index
                            // and block offset bits.
    int set_index = (int)((address >> b) &
                          ((1L << s) - 1)); // Extract set index bits by
                                            // discarding the block offset bits.

    int line_found = 0; // Flag to indicate if the line was found in the cache.
    long empty_line = -1; // Keep track of the first empty line.

    // Check each line in the set.
    for (long i = 0; i < E; i++) {
        // If the line is valid...
        if (c[set_index][i].valid) {
            // And the tag matches...
            if (c[set_index][i].tag == tag) {
                // It's a cache hit.
                line_found = 1;
                stats->hits++;

                // If it's a modify operation, it's a double hit.
                if (operation == 'M') {
                    stats->hits++;
                }
                if (verbose) {
                    printf("%c %lx,%d hit\n", operation, address, (1 << b));
                }

                // Reset the LRU counter for this line.
                c[set_index][i].lru = 0;

                // If it's a modify or store operation, set the dirty bit.
                if (operation == 'M' || operation == 'S') {
                    c[set_index][i].dirty = 1;
                }

                break;
            }
        } else if (empty_line == -1) {
            // If the line is not valid, remember it as a potential place to
            // bring in new data.
            empty_line = i;
        }
    }

    // If the line was not found in the cache...
    if (!line_found) {
        stats->misses++;

        // If it's a modify operation, it's a hit after the miss.
        if (operation == 'M') {
            stats->hits++;
        }

        if (empty_line != -1) {
            // If there's an empty line, use it to bring in the new data.
            c[set_index][empty_line].valid = 1;
            c[set_index][empty_line].tag = tag;
            c[set_index][empty_line].lru = 0; // Reset LRU.
            if (operation == 'M' || operation == 'S') {
                c[set_index][empty_line].dirty =
                    1; // Set dirty bit if modify or store.
            }
            if (verbose) {
                printf("%c %lx,%d miss\n", operation, address, (1 << b));
            }
        } else {
            // Otherwise, evict the least recently used line.
            stats->evictions++;
            long max_lru_index = 0;
            for (long i = 1; i < E; i++) {
                if (c[set_index][i].lru > c[set_index][max_lru_index].lru) {
                    max_lru_index = i;
                }
            }

            // If evicting a dirty line, update the dirty eviction stats.
            if (c[set_index][max_lru_index].dirty) {
                stats->dirty_evictions += (1 << b);
                c[set_index][max_lru_index].dirty = 0; // Reset dirty flag.
            }

            // Update the evicted line's metadata for the new data.
            c[set_index][max_lru_index].valid = 1;
            c[set_index][max_lru_index].tag = tag;
            c[set_index][max_lru_index].lru = 0; // Reset LRU.
            if (operation == 'M' || operation == 'S') {
                c[set_index][max_lru_index].dirty =
                    1; // Set dirty bit if modify or store.
            }
            if (verbose) {
                printf("%c %lx,%d miss eviction\n", operation, address,
                       (1 << b));
            }
        }
    }

    // After the access, increment the LRU counter for each line in the set.
    for (int i = 0; i < E; i++) {
        c[set_index][i].lru++;
    }
}
