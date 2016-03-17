#ifndef _ISI_CPUTYPES_H_
#define _ISI_CPUTYPES_H_

#include <stdint.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/time.h>

struct isiCPUInfo;

typedef int (*CPUECall)(struct isiCPUInfo *, struct timespec crun);

struct isiCPUInfo {
	int archtype;
	int ctl;
	size_t rate;
	size_t runrate;
	size_t itvl;
	void * cpustate;
	CPUECall RunCycles;
	struct timespec nrun;
	size_t cycl;
};

struct isiSession {
	int sid;
	int sfd;
	struct sockaddr_in r_addr;
};

void isi_addtime(struct timespec *, size_t nsec);
int HWM_TickAll(DCPU *, struct timespec, int fdnet, int msgin);

#define ISICTL_DEBUG  ( 1 << 0 )
#define ISICTL_STEP   ( 1 << 1 )
#define ISICTL_STEPE  ( 1 << 2 )

#define ARCH_NONE 0
#define ARCH_DCPU 1

#endif

