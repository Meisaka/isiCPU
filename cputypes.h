#ifndef _ISI_CPUTYPES_H_
#define _ISI_CPUTYPES_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>

struct isiCPUInfo;

typedef int (*CPUECall)(struct isiCPUInfo *, struct timespec crun);

typedef struct isiCPUInfo {
	int archtype;
	size_t memsize;
	size_t rate;
	size_t runrate;
	size_t itvl;
	void * cpustate;
	void * memptr;
	int ctl;
	CPUECall RunCycles;
	struct timespec nrun;
	size_t cycl;
	uint64_t cyclequeue;
	uint64_t cyclewait;
} CPUSlot, *pCPUSlot;

void isi_addtime(struct timespec *, size_t nsec);
int HWM_TickAll(DCPU *, struct timespec, int fdnet, int msgin);

#define ISICTL_DEBUG  ( 1 << 0 )

#define ARCH_NONE 0
#define ARCH_DCPU 1

#endif

