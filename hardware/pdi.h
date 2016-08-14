
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>
#include "../cputypes.h"

#define PDI_IBUSY 0x1000
#define PDI_OVFLAG 0x2000
#define PDI_BUFLIMIT 3
#define PDI_MASKC 0x000f
#define PDI_MASKR 0x00f0
#define PDI_GETC(a) ((a) & 0x0f)
#define PDI_GETR(a) (((a) >> 4) & 0x0f)
#define PDI_SETC(a) ((a) & 0x0f)
#define PDI_SETR(a) (((a) & 0x0f) << 4)

struct PDI_port {
	uint16_t stat;
	uint16_t dat[PDI_BUFLIMIT];
};

int pdi_addrxword(struct PDI_port *p, struct timespec *pt, uint16_t word, struct timespec const *now);
int pdi_addtxword(struct PDI_port *p, struct timespec *pt, uint16_t word, struct timespec const *now);
int pdi_getword(struct PDI_port *p, uint16_t *word);
int pdi_getrxoverflow(struct PDI_port *p);
int pdi_isbusy(struct PDI_port *p);
int pdi_hasdata(struct PDI_port *p);
int pdi_hasfree(struct PDI_port *p);
int pdi_process(struct PDI_port *p, struct timespec *pt, struct timespec const *now);

