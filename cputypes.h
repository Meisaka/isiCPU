#ifndef _ISI_CPUTYPES_H_
#define _ISI_CPUTYPES_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>

struct isiCPUInfo;

typedef int (*CPUECall)(struct isiCPUInfo *, void * cpustate, void * mem);

typedef struct isiCPUInfo {
	int archtype;
	size_t memsize;
	size_t rate;
	size_t runrate;
	void * cpustate;
	void * memptr;
	CPUECall RunCycles;
	struct timespec lrun;
	uint32_t nse;
	size_t cycl;
	uint64_t cyclequeue;
	uint64_t cyclewait;
} CPUSlot, *pCPUSlot;

#define ARCH_NONE 0
#define ARCH_DCPU 1

#endif

