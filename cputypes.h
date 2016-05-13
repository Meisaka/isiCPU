#ifndef _ISI_CPUTYPES_H_
#define _ISI_CPUTYPES_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/time.h>
#include "reflect.h"

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
	uint32_t rcv;
	uint8_t *in;
	uint8_t *out;
};

struct isiSessionTable {
	struct isiSession ** table;
	uint32_t count;
	uint32_t limit;
	struct pollfd * ptable;
	uint32_t pcount;
};

struct isiObjTable {
	struct objtype ** table;
	uint32_t count;
	uint32_t limit;
};

struct isiDevTable {
	struct isiInfo ** table;
	uint32_t count;
	uint32_t limit;
};

struct isiNetSync {
	struct objtype id;
	struct objtype target;
	struct objtype memobj;
	int synctype;
	int ctl;
	int target_index;
	int memobj_index;
	size_t rate;
	struct timespec nrun;
	uint32_t extents;
	uint32_t base[4];
	uint32_t len[4];
};

typedef int (*isi_run_call)(struct isiInfo *, struct timespec crun);
typedef int (*isi_control_call)(struct isiInfo *);
typedef int (*isi_attach_call)(struct isiInfo *to, struct isiInfo *dev);
typedef int (*isi_message_call)(struct isiInfo *dest, struct isiInfo *src, uint16_t *, struct timespec mtime);

struct isiInfo {
	struct objtype id;
	isi_run_call RunCycles;
	isi_message_call MsgIn;
	isi_message_call MsgOut;
	isi_attach_call QueryAttach;
	isi_attach_call Attach;
	isi_attach_call Attached;
	isi_control_call Reset;
	isi_control_call Delete;
	struct isiInfo *updev;
	struct isiInfo *dndev;
	void * rvstate;
	void * svstate;
	struct isiReflection *rvproto;
	struct isiReflection *svproto;
	void * mem;
	struct isiInfo * hostcpu;
	struct timespec nrun;
};

struct isiBusInfo SUBCLASS(isiInfo) {
	/* isiInfo */
#ifndef __cplusplus
	struct isiInfo i;
#endif
	struct isiDevTable busdev;
};

struct isiDisk SUBCLASS(isiInfo) {
	/* isiInfo */
#ifndef __cplusplus
	struct isiInfo i;
#endif
	int fd;
	uint64_t diskid;
	uint32_t block;
};
struct disk_svstate {
	char block[4096];
	char dblock[4096];
};
struct isiDiskSeekMsg {
	uint16_t mcode;
	uint16_t ex;
	uint32_t block;
};

struct isiCPUInfo SUBCLASS(isiInfo) {
	/* isiInfo */
#ifndef __cplusplus
	struct objtype id;
	struct isiInfo i;
#endif
	/* CPU specific */
	int ctl;
	size_t rate;
	size_t runrate;
	size_t itvl;
	size_t cycl;
};

typedef struct memory64x16 {
	struct objtype id;
	uint16_t ram[0x10000];
	uint32_t ctl[0x10000];
	uint32_t info;
} *isiram16;

#define ISI_RAMCTL_DELTA (1<<16)
#define ISI_RAMCTL_SYNC (1<<17)
#define ISI_RAMCTL_RDONLY (1<<18)
#define ISI_RAMINFO_SCAN 1

uint16_t isi_cpu_rdmem(isiram16 ram, uint16_t a);
void isi_cpu_wrmem(isiram16 ram, uint16_t a, uint16_t v);
uint16_t isi_hw_rdmem(isiram16 ram, uint16_t a);
void isi_hw_wrmem(isiram16 ram, uint16_t a, uint16_t v);

/* attach dev to item */
int isi_attach(struct isiInfo *item, struct isiInfo *dev);
int isi_create_object(int objtype, struct objtype **out);
int isi_create_disk(uint64_t diskid, struct isiInfo **ndisk);
int isi_createdev(struct isiInfo **ndev);
int isi_pushdev(struct isiDevTable *t, struct isiInfo *d);

void isi_addtime(struct timespec *, size_t nsec);
int isi_time_lt(struct timespec *, struct timespec *);

int isi_inittable(struct isiDevTable *t);

int loadbinfile(const char* path, int endian, unsigned char **nmem, uint32_t *nsize);
int savebinfile(const char* path, int endian, unsigned char *nmem, uint32_t nsize);
int isi_text_dec(const char *text, int len, int limit, void *vv, int olen);
int isi_disk_getblock(struct isiInfo *disk, void **blockaddr);

int session_write(struct isiSession *, int len);
int session_write_msg(struct isiSession *);
int session_write_msgex(struct isiSession *, void *);
int session_write_buf(struct isiSession *, void *, int len);

void isi_synctable_init();
void isi_debug_dump_synctable();
int isi_add_memsync(struct objtype *target, uint32_t base, uint32_t extent, size_t rate);
int isi_add_sync_extent(struct objtype *target, uint32_t base, uint32_t extent);
int isi_set_sync_extent(struct objtype *target, uint32_t index, uint32_t base, uint32_t extent);
int isi_add_devsync(struct objtype *target, size_t rate);
int isi_add_devmemsync(struct objtype *target, struct objtype *memtarget, size_t rate);
int isi_set_devmemsync_extent(struct objtype *target, struct objtype *memtarget, uint32_t index, uint32_t base, uint32_t extent);
int isi_resync_dev(struct objtype *target);
int isi_resync_all();
int isi_remove_sync(struct objtype *target);

#define ISIN_SYNC_NONE 0
#define ISIN_SYNC_DEVR 1
#define ISIN_SYNC_DEVV 2
#define ISIN_SYNC_DEVRV 3
#define ISIN_SYNC_MEM 4
#define ISIN_SYNC_MEMDEV 5

#define ISICTL_DEBUG  ( 1 << 0 )
#define ISICTL_STEP   ( 1 << 1 )
#define ISICTL_STEPE  ( 1 << 2 )

#define ISIT_NONE      0
#define ISIT_SESSION   0x1000
#define ISIT_NETSYNC   0x1001
#define ISIT_MEM6416   0x2001
#define ISIT_DISK      0x2fff
#define ISIT_CPU       0x3000
#define ISIT_DCPU      0x3001
#define ISIT_ENDCPU    0x4000
#define ISIT_BUSDEV    0x4000
#define ISIT_DCPUBUS   0x4001
#define ISIT_ENDBUSDEV 0x5000
#define ISIT_DCPUHW    0x5000

void DCPU_init(struct isiInfo *, isiram16 ram);

#endif

