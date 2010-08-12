// curx.h
//
// Author: Berke Durak <berke.durak@gmail.com>
// vim:set ts=2 sw=2 foldmarker={,}:

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

#ifndef CURX_MISS_CHECKER_LG2_WINDOW
  #define CURX_MISS_CHECKER_LG2_WINDOW 7
#endif

#if CURX_MISS_CHECKER_LG2_WINDOW < 1
  #error "CURX_MISS_CHECKER_LG2_WINDOW must be at least 1"
#endif

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
   CURX_MIN   = 0,
   CURX_OK    = 0,
   CURX_SHORT = 1,
   CURX_BAD   = 2,
   CURX_OOO   = 4,
   CURX_DUP   = 8,
   CURX_TRUNC = 16,
   CURX_BER   = 32,
   CURX_MAX   = 63
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
   void (*output_missing_hook)(void *, uint32_t, uint32_t, uint32_t);
   void *hook_data;
};

void curx_init(struct curx_state *q, void (*output_missing_hook)(void *, uint32_t, uint32_t, uint32_t), void *hook_data);

/* q      - properly initialized state
 * data   - Pointer to UDP payload data
 * length - Size of UDP payload
 */
enum curx_status curx_receive(struct curx_state *q, const char *buffer, const size_t m0);

#define CURX_STATUS_FORMAT "{%s%s%s%s%s%s }"
#define CURX_STATUS_FORMAT_PADDED "{%6s%6s%6s%6s%6s%6s }"
#define CURX_STATUS_ARGS(status) \
            status & CURX_SHORT ? " short":"", \
            status & CURX_BAD   ? " bad"  :"", \
            status & CURX_OOO   ? " ooo"  :"", \
            status & CURX_DUP   ? " dup"  :"", \
            status & CURX_TRUNC ? " trunc":"", \
            status & CURX_BER   ? " ber"  :""

#endif
