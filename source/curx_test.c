/* curx_test.c */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "curx.h"

#define avoid(x, msg, args...) \
  do { \
    if( x ) \
    { \
      fprintf(stderr, "Error at %s:%d: %s: " msg "\n", \
          __FILE__, __LINE__, \
          strerror(errno), \
          ##args); \
      fflush(stderr); \
      exit(EXIT_FAILURE); \
    }; \
  } while(0)

static void display_packet(const char *data, int m)
{
   int i;
   for(i = 0; i < m; i ++)
   {
      if((i & 7) == 0)
      {
         if(i > 0) printf("\n");
         printf("%04x: ", i);
      }
      printf(" %02x", (unsigned char) data[i]);
   }
   if(m > 0) printf("\n");
}

void output_missing(uint32_t count, uint32_t first, uint32_t last)
{
   /* printf("Missing %u from %u to %u\n", count, first, last); */
}

int main(int argc, char **argv)
{
   int s;
   const int port = 33333;
   struct sockaddr_in sin, sin_old;
   char data[1500];
   ssize_t m;
   socklen_t sin_len;
   struct curx_state cx;
   int count = 0;
   bool have_sin_old = false;

   void display()
   {
      unsigned long long loss_ratio = (1000000 * cx.mc.missing) / (cx.mc.missing + cx.mc.original);

      printf("RX statistics:\n");
      printf("  Total packets ............................ %Lu pk\n",  cx.count);
      printf("  Total bytes .............................. %Lu B\n",   cx.byte_count);
      printf("  Packets with bad checksum ................ %Lu pk\n",  cx.bad_checksum);
      printf("  Truncated packets ........................ %Lu pk\n",  cx.truncated);
      printf("  Lowest sequence # ........................ %Lu pk\n",  cx.seq_min);
      printf("  Highest sequence # ....................... %Lu pk\n",  cx.seq_max);
      printf("  Out of order packets ..................... %Lu pk\n",  cx.out_of_order);
      printf("  Decodable packets ........................ %Lu pk\n",  cx.decodable_count);
      printf("  Decodable loss ratio ..................... %Lu ppm\n", loss_ratio);
      printf("  Original decodables ...................... %Lu pk\n",  cx.mc.original);
      printf("  Lost decodables .......................... %Lu pk\n",  cx.mc.missing);
      printf("  Duplicate decodables ..................... %Lu pk\n",  cx.mc.duplicates);
      printf("  Payload byte errors ...................... %Lu B\n",   cx.total_errors);
      printf("  Decodables with erroneous payloads........ %Lu pk\n",  cx.total_erroneous);
   }

   s = socket(AF_INET, SOCK_DGRAM, 0);
   avoid(s < 0, "Can't open socket");
   sin.sin_addr.s_addr = INADDR_ANY;
   sin.sin_port = htons(port);
   avoid(bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0, "Can't bind to port %d", port);
   curx_output_missing_hook = output_missing;

   while(true)
   {
      m = sizeof(data);
      sin_len = sizeof(sin);
      m = recvfrom(s, data, sizeof(data), 0, (struct sockaddr *) &sin, &sin_len);
      avoid(m < 0, "Error receiving on socket");
      count ++;

      if(!have_sin_old || sin.sin_addr.s_addr != sin_old.sin_addr.s_addr || sin.sin_port != sin_old.sin_port)
      {
         if(have_sin_old) display();
         curx_init(&cx);
         memcpy(&sin_old, &sin, sizeof(sin_old));
         have_sin_old = true;
         printf("Receiving from %lx:%d\n", ntohl(sin.sin_addr.s_addr), ntohs(sin.sin_port));
      }

      /* printf("Received %d bytes on socket:\n", m);
         display_packet(data, m); */
      curx_receive(&cx, data, m);
      if(count % 1000 == 0)
      {
         display();
      }
   }

   return 0;
}
