
#include <sys/types.h>
#include <sys/time.h>

typedef int (*CPUECall)(void * cpustate, void * mem);

typedef struct {
	int archtype;
	int memsize;
	int rate;
	int runrate;
	void * cpustate;
	void * memptr;
	CPUECall RunCycles;
	struct timespec lrun;
	long long cyclequeue;
} CPUSlot, *pCPUSlot;

#define ARCH_NONE 0
#define ARCH_DCPU 1

