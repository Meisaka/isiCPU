#ifndef _ISI_CPUTYPES_H_
#define _ISI_CPUTYPES_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
#define SUBCLASS(z) : public z
#else
#define SUBCLASS(z)
#endif
struct isiInfo;
struct isiCPUInfo;

struct objtype {
	uint32_t objtype;
	uint32_t id;
};

struct isiSession {
	struct objtype id;
	int sfd;
	struct sockaddr_in r_addr;
};

struct isiDevTable {
	struct isiInfo ** table;
	uint32_t count;
	uint32_t limit;
};

typedef int (*isi_run)(struct isiInfo *, struct isiSession *, struct timespec crun);
typedef int (*isi_control)(struct isiInfo *);
typedef int (*isi_attach)(struct isiInfo *to, struct isiInfo *dev);
typedef int (*isi_message)(struct isiInfo *dest, struct isiInfo *src, uint16_t *, struct timespec mtime);

struct isiInfo {
	struct objtype id;
	isi_run RunCycles;
	isi_message MsgIn;
	isi_message MsgOut;
	isi_attach Attach;
	isi_control Reset;
	struct isiInfo *outdev;
	void * rvstate;
	void * svstate;
	struct timespec nrun;
};

struct isiBusInfo SUBCLASS(isiInfo) {
	/* isiInfo */
#ifndef __cplusplus
	struct objtype id;
	struct isiInfo i;
#endif
	struct isiDevTable busdev;
};

struct isiCPUInfo SUBCLASS(isiInfo) {
	/* isiInfo */
#ifndef __cplusplus
	struct objtype id;
	struct isiInfo i;
#endif
	/* CPU specific */
	void * mem;
	int ctl;
	size_t rate;
	size_t runrate;
	size_t itvl;
	size_t cycl;
};

typedef struct memory64x16 {
	struct objtype id;
	uint16_t ram[0x10000];
	uint16_t ctl[0x10000];
	uint32_t info;
} *isiram16;

void isi_addtime(struct timespec *, size_t nsec);
int isi_inittable(struct isiDevTable *t);
int isi_createdev(struct isiInfo **ndev);
int isi_pushdev(struct isiDevTable *t, struct isiInfo *d);

#define ISICTL_DEBUG  ( 1 << 0 )
#define ISICTL_STEP   ( 1 << 1 )
#define ISICTL_STEPE  ( 1 << 2 )

#define ARCH_NONE      0
#define ARCH_DCPU      0x3001
#define ARCH_DCPUBUS   0x4001
#define ARCH_DCPUHW    0x5000

void DCPU_init(struct isiInfo *, isiram16 ram);

#endif

