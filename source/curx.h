#ifndef CURX_H
#define CURX_H

#include "curx_config.h"

struct curx_ph
{
   uint32_t sequence;
   uint32_t timestamp;
   uint16_t size;
   uint16_t check;
};

#define CURX_MISS_CHECKER_LG2_WINDOW 5
#define CURX_MISS_CHECKER_WINDOW (1 << CURX_MISS_CHECKER_LG2_WINDOW)
#define CURX_MISS_CHECKER_WINDOW_MASK (CURX_MISS_CHECKER_WINDOW - 1)

struct curx_miss_checker_result
{
   int is_duplicate;
   int some_missing;
   uint32_t first_missing, last_missing;
};

struct curx_miss_checker
{
   uint64_t duplicates, missing, original;
   uint32_t seen[CURX_MISS_CHECKER_WINDOW];
   unsigned int seen_start, seen_size;
   struct curx_miss_checker_result result;
};

enum curx_status
{
   CURX_OK    = 0,
   CURX_SHORT = 1,
   CURX_BAD   = 2,
   CURX_OOO   = 4,
   CURX_DUP   = 8,
   CURX_TRUNC = 16,
   CURX_BER   = 32
};

struct curx_wprng
{
  uint32_t a, b, c, d;
};

struct curx_state
{
   struct curx_wprng rng;
   uint64_t seq_min, seq_max, seq_last, out_of_order, count, decodable_count,
            byte_count, bad_checksum, truncated, total_errors, total_erroneous;
   struct curx_miss_checker mc;
};

extern void (*curx_output_missing_hook)(uint32_t, uint32_t, uint32_t);

void curx_init(struct curx_state *q);

/* q      - properly initialized state
 * data   - Pointer to UDP payload data
 * length - Size of UDP payload
 */
enum curx_status curx_receive(struct curx_state *q, const char *buffer, const size_t m0);

#endif
