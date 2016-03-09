
#ifndef _ISI_DCPU_H_
#define _ISI_DCPU_H_

#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct stDCPU {
	uint16_t R[8];
	uint16_t PC;
	uint16_t SP;
	uint16_t EX;
	uint16_t IA;

	unsigned long long cycl;
	int MODE;
	int control;
	int cpuid;
	int hwman;
	int hwmem;
	uint16_t *memptr;
	uint16_t hwcount;
	void * hwloadout;
	void * hwdata;

	//DCPUext_query hwqaf;
	int (*hwqaf)(uint16_t*, uint16_t hwnum, struct stDCPU *cpu);
	//DCPUext_interupt hwiaf;
	int (*hwiaf)(uint16_t*, uint16_t hwnum, struct stDCPU *cpu);
	/* Interupt queue */
	int IQC;
	uint16_t IQU[256];
} DCPU;

typedef int (*DCPUext_query)(uint16_t*, uint16_t hwnum, DCPU *cpu);
typedef int (*DCPUext_interupt)(uint16_t*, uint16_t hwnum, DCPU *cpu);

#define HUGE_FIREBALL (-3141592)
#define BURNING (0xB19F14E)

struct isiCPUInfo;

void DCPU_init(struct isiCPUInfo *, uint16_t * ram);
void DCPU_reset(DCPU* pr);
int DCPU_run(struct isiCPUInfo *, struct timespec);
int DCPU_interupt(DCPU* pr, uint16_t msg);
int DCPU_sethwcount(DCPU* pr, uint16_t count);
int DCPU_sethwqcallback(DCPU*, DCPUext_query);
int DCPU_sethwicallback(DCPU*, DCPUext_interupt);

#endif
