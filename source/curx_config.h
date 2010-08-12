#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#define curx_printf(fmt,args...) do{printf("CURX: " fmt, ##args);}while(0)
