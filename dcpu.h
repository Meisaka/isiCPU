
#ifndef _ISI_DCPU_H_
#define _ISI_DCPU_H_

#include "cputypes.h"

typedef struct stDCPU {
	uint16_t msg;
	uint16_t dai;
	uint16_t R[8];
	uint16_t PC;
	uint16_t SP;
	uint16_t EX;
	uint16_t IA;

	unsigned long long cycl;
	int MODE;
	isiram16 memptr;
	uint16_t hwcount;
	/* Interupt queue */
	int IQC;
	uint16_t IQU[256];
} DCPU;

#define HUGE_FIREBALL (-3141592)
#define BURNING (0xB19F14E)

#endif
