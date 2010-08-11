#include <inttypes.h>
#include <stddef.h>
#include <arpa/inet.h>

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
   CURX_TRUNC = 16
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

void (*curx_output_missing_hook)(uint32_t, uint32_t, uint32_t) = NULL;

static inline uint32_t curx_wprng_rol32(uint32_t x, uint32_t y)
{
   y &= 31;
   return (x << y) | (x >> (32 - y));
}

void curx_wprng_init(struct curx_wprng *g, uint32_t seed)
{
   g->a = 0xdeadbeef ^ seed;
   g->b = 0x0badcafe + seed;
   g->c = 0xdeadface - seed;
   g->d = 0xdefaced1 ^ seed;
}

void curx_wprng_step(struct curx_wprng *g)
{
   g->a = curx_wprng_rol32(g->a, g->d);
   g->b ^= 0x89abcdef;
   g->c = curx_wprng_rol32(g->c, g->b);
   g->d = curx_wprng_rol32(g->d, g->a);
   g->c ^= 0x31415926;
   g->a ^= 0x01234567;
   g->b = curx_wprng_rol32(g->b, g->c);
   g->a += g->c;
   g->b ^= g->d;
   g->c -= g->a;
   g->d -= g->b;
   g->d ^= 0x54581414;
}

uint32_t curx_wprng_get(struct curx_wprng *g)
{
   curx_wprng_step(g);
   return g->a;
};

void curx_init(struct curx_state *q)
{
   q->seq_min         = 0;
   q->seq_max         = 0;
   q->seq_last        = 0;
   q->out_of_order    = 0;
   q->count           = 0;
   q->decodable_count = 0;
   q->byte_count      = 0;
   q->bad_checksum    = 0;
   q->truncated       = 0;
   q->total_errors    = 0;
   q->total_erroneous = 0;
}

struct curx_ph
{
   uint32_t sequence;
   uint32_t timestamp;
   uint16_t size;
   uint16_t check;
};

void curx_ph_fix_endianness(struct curx_ph *h)
{
   h->sequence  = ntohl(h->sequence);
   h->timestamp = ntohl(h->timestamp);
   h->size      = ntohs(h->size);
   h->check     = ntohs(h->check);
}

uint16_t curx_ph_get_checksum(struct curx_ph *h)
{
   return ~(h->sequence ^ h->size ^ h->timestamp);
}

int curx_ph_checksum_valid(struct curx_ph *h)
{
   return curx_ph_get_checksum(h) == h->check;
}

void curx_miss_checker_init(struct curx_miss_checker *c)
{
   c->duplicates = 0;
   c->missing    = 0;
   c->original   = 0;
   c->seen_start = 0;
   c->seen_size  = 0;
}

void curx_miss_checker_insert(struct curx_miss_checker *c, unsigned int seq)
{
   c->seen[(c->seen_start + c->seen_size) & CURX_MISS_CHECKER_WINDOW_MASK] = seq;
   c->seen_size ++;
}

void curx_miss_checker_remove_oldest(struct curx_miss_checker *c, struct curx_miss_checker_result *r)
{
   if(c->seen_size >= 2)
   {
      uint32_t s0 = c->seen[c->seen_start],
               s1 = c->seen[(c->seen_start + 1) & CURX_MISS_CHECKER_WINDOW_MASK];
      unsigned int num_missing = s1 - s0 - 1;
      if(num_missing > 0)
      {
        c->missing += num_missing;
        r->some_missing = 1;
        r->first_missing = s0 + 1;
        r->last_missing = s1 - 1;
      }
   }
   if(c->seen_size > 0)
   {
      c->seen_start ++;
      c->seen_size --;
   }
}

void curx_miss_checker_add(struct curx_miss_checker *c, uint32_t seq)
{
   unsigned int i;
   struct curx_miss_checker_result *r = &c->result;

   r->is_duplicate = 0;
   r->some_missing = 0;

   for(i = 0; i < c->seen_size; i ++)
   {
      if(c->seen[(c->seen_start + i) & CURX_MISS_CHECKER_WINDOW_MASK] == seq) break;
   }

   if(i == c->seen_size)
   {
      c->original ++;
      if(c->seen_size == CURX_MISS_CHECKER_WINDOW) curx_miss_checker_remove_oldest(c, r);
      curx_miss_checker_insert(c, seq);
   }
   else
   {
      c->duplicates ++;
      r->is_duplicate = 1;
   }
}

/* data   - Pointer to UDP payload data
 * length - Size of UDP payload
 */

enum curx_status curx_receive(struct curx_state *q, const char *buffer, const size_t m0)
{
   enum curx_status status = CURX_OK;
   uint32_t seq = 0;
   uint32_t errors = 0;
   unsigned int i;
   size_t m = m0;
   struct curx_ph *ph;
   struct curx_wprng *w = &q->rng;
   char *p;

   do
   {
      if(m0 < sizeof(struct curx_ph))
      {
         status = CURX_SHORT;
         break;
      }

      ph = (struct curx_ph *) buffer;
      curx_ph_fix_endianness(ph);
      if(!curx_ph_checksum_valid(ph))
      {
         status = CURX_BAD;
         q->bad_checksum ++;
         break;
      }

      seq = ph->sequence;
      if(!q->count || seq < q->seq_min) q->seq_min = seq;
      if(!q->count || seq > q->seq_max) q->seq_max = seq;
      if(q->count && seq < q->seq_last)
      {
         status |= CURX_OOO;
         q->out_of_order ++;
      }
      q->seq_last = seq;

      curx_miss_checker_add(&q->mc, seq);
      if(q->mc.result.is_duplicate) status |= CURX_DUP;
      if(q->mc.result.some_missing)
      {
         if(curx_output_missing_hook != NULL)
            curx_output_missing_hook(
                                q->mc.result.last_missing - q->mc.result.first_missing + 1,
                                q->mc.result.first_missing,
                                q->mc.result.last_missing
                              );
      }

      curx_wprng_init(w, ph->check);

      if(ph->size != m)
      {
         q->truncated ++;
         status |= CURX_TRUNC;
         break;
      }

      q->decodable_count ++;

      p = (char *) (ph + 1);

      for(i = 0; i < m; i ++)
      {
         uint8_t expected_byte, received_byte;

         expected_byte = curx_wprng_get(w);
         received_byte = *(p ++);

         if(expected_byte != received_byte) errors ++;
      }
      if(errors > 0)
      {
         q->total_erroneous ++;
         q->total_errors += errors;
      }
   }
   while(0);

   q->byte_count += m0;
   q->count ++;

   return status;
   /* log << t_rx << " " << m0 << " " << status << " " << seq << " " << t_tx << " " << errors << "\n";*/
}
