#ifndef _ISI_CPUTYPES_H_
#define _ISI_CPUTYPES_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/time.h>
#include "reflect.h"
#include "isidefs.h"

#ifdef __cplusplus
#define SUBCLASS(z) : public z
#else
#define SUBCLASS(z)
#endif
struct isiInfo;
struct isiCPUInfo;
struct isiSession;

struct objtype {
	uint32_t objtype;
	uint32_t id;
	uint64_t uuid;
};

struct sescommandset {
	uint32_t cmd;
	uint32_t tid; /* tx ID */
	uint32_t id; /* session ID for tx commands */
	uint32_t param;
	void * rdata; /* alloc/free */
	void * xdata; /* alloc/free */
	void * cptr; /* raw pointer, not freed */
};

typedef int (*isi_ses_handle)(struct isiSession *ses, struct timespec mtime);
typedef int (*isi_ses_cmdend)(struct isiSession *ses, struct sescommandset *cmd, int result);

struct isiSession {
	struct objtype id;
	int stype;
	int sfd;
	struct sockaddr_in r_addr;
	uint32_t rcv;
	uint8_t *in;
	uint8_t *out;
	void * istate;
	struct timespec rqtimeout;
	struct sescommandset *cmdq;
	struct isiInfo *ccmei;
	uint32_t cmdqstart;
	uint32_t cmdqend;
	uint32_t cmdqlimit;
	isi_ses_handle Recv;
	isi_ses_handle STick;
	isi_ses_handle LTick;
	isi_ses_cmdend AsyncDone;
};

struct isiSessionRef {
	uint32_t id;
	uint32_t index;
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

struct isiConPoint {
	struct isiInfo *t;
	int32_t i;
};

struct isiDevTable {
	struct isiInfo ** table;
	uint32_t count;
	uint32_t limit;
};

struct isiDevTree {
	struct isiConPoint * table;
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

typedef int (*isi_init_call)(struct isiInfo *);
typedef int (*isi_size_call)(int, size_t *);
typedef int (*isi_new_call)(struct isiInfo *, const uint8_t *, size_t);
typedef int (*isi_run_call)(struct isiInfo *, struct timespec crun);
typedef int (*isi_control_call)(struct isiInfo *);
typedef int (*isi_attach_call)(struct isiInfo *to, int32_t topoint, struct isiInfo *dev, int32_t frompoint);
typedef int (*isi_message_call)(struct isiInfo *dest, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime);

struct isiConstruct {
	uint32_t objtype;
	const char * name;
	const char * desc;
	isi_init_call PreInit;
	isi_init_call Init;
	isi_size_call QuerySize;
	isi_new_call New;
	struct isiReflection *rvproto;
	struct isiReflection *svproto;
	void * meta;
};

struct isiConTable {
	struct isiConstruct ** table;
	uint32_t count;
	uint32_t limit;
};

struct isiInfoCalls {
	isi_run_call RunCycles;
	isi_message_call MsgIn;
	isi_attach_call QueryAttach;
	isi_attach_call Attach;
	isi_attach_call Attached;
	isi_attach_call Deattach;
	isi_control_call Reset;
	isi_control_call Delete;
};

struct isiInfo {
	struct objtype id;
	struct isiInfoCalls *c;
	struct isiSessionRef sesref;
	struct isiConPoint updev;
	void * rvstate;
	void * svstate;
	void * nvstate;
	struct isiReflection *rvproto;
	struct isiReflection *svproto;
	size_t nvsize;
	void * mem;
	const struct isiConstruct * meta;
	struct timespec nrun;
	struct isiDevTree busdev;
};

struct isiDisk SUBCLASS(isiInfo) {
	/* isiInfo */
#ifndef __cplusplus
	struct isiInfo i;
#endif
	int fd;
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
	size_t rcycl;
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
#define ISI_RAMCTL_BREAK (1<<19)
#define ISI_RAMINFO_SCAN 1

uint16_t isi_cpu_rdmem(isiram16 ram, uint16_t a);
void isi_cpu_wrmem(isiram16 ram, uint16_t a, uint16_t v);
uint16_t isi_hw_rdmem(isiram16 ram, uint16_t a);
void isi_hw_wrmem(isiram16 ram, uint16_t a, uint16_t v);
int isi_cpu_isbrk(isiram16 ram, uint16_t a);
void isi_cpu_togglebrk(isiram16 ram, uint16_t a);

/* attach dev to item */
int isi_attach(struct isiInfo *item, int32_t itempoint, struct isiInfo *dev, int32_t devpoint, int32_t *itemactual_out, int32_t *devactual_out);
int isi_deattach(struct isiInfo *item, int32_t itempoint);
int isi_make_object(int objtype, struct objtype **out, const uint8_t *cfg, size_t lcfg);
int isi_create_object(int objtype, struct objtype **out);
int isi_delete_object(struct objtype *obj);
int isi_find_obj(uint32_t id, struct objtype **target);
int isi_find_uuid(uint32_t cid, uint64_t uuid, struct objtype **target);
int isi_createdev(struct isiInfo **ndev);
int isi_push_dev(struct isiDevTable *t, struct isiInfo *d);
int isi_find_dev(struct isiDevTable *t, uint32_t id, struct isiInfo **target);
int isi_getindex_devi(struct isiInfo *dev, uint32_t index, struct isiInfo **downdev, int32_t *downidx);
int isi_getindex_dev(struct isiInfo *dev, uint32_t index, struct isiInfo **downdev);
int isi_message_dev(struct isiInfo *src, int32_t srcindex, uint16_t *, int, struct timespec mtime);
uint32_t isi_lookup_name(const char *);
int isi_get_name(uint32_t cid, const char **name);
int isi_write_parameter(uint8_t *p, int plen, int code, const void *in, int limit);
int isi_fetch_parameter(const uint8_t *p, int plen, int code, void *out, int limit);
void * isi_realloc(void *h, size_t mem);
void * isi_alloc(size_t mem);
void * isi_alloc_array(size_t count, size_t elemsize);
int isi_text_enc(char *text, int limit, void const *vv, int len);

void fetchtime(struct timespec * t);
void isi_addtime(struct timespec *, size_t nsec);
int isi_time_lt(struct timespec *, struct timespec *);
void isi_setrate(struct isiCPUInfo *info, size_t rate);

#define L_DEBUG 5
#define L_NOTE 4
#define L_INFO 3
#define L_WARN 2
#define L_ERR 1
#define L_CRIT 0
void isilogerr(const char * desc);
void isilog(int level, const char *format, ...);

int isi_register(struct isiConstruct *obj);
int isi_inittable(struct isiDevTable *t);

int loadbinfile(const char* path, int endian, unsigned char **nmem, uint32_t *nsize);
int loadbinfileto(const char* path, int endian, unsigned char *nmem, uint32_t nsize);
int savebinfile(const char* path, int endian, unsigned char *nmem, uint32_t nsize);
int isi_text_dec(const char *text, int len, int limit, void *vv, int olen);
int isi_disk_getblock(struct isiInfo *disk, void **blockaddr);
int isi_disk_getindex(struct isiInfo *disk, uint32_t *blockindex);
int isi_disk_isreadonly(struct isiInfo *disk);
int isi_find_bin(uint64_t diskid, char **nameout);
size_t isi_fsize(const char *path);
int isi_fname_id(const char *fname, uint64_t *id);
int isi_text_dec(const char *text, int len, int limit, void *vv, int olen);

int session_write(struct isiSession *, int len);
int session_write_msg(struct isiSession *);
int session_write_msgex(struct isiSession *, void *);
int session_write_buf(struct isiSession *, void *, int len);
int isi_pushses(struct isiSession *s);
int session_get_cmdq(struct isiSession *ses, struct sescommandset **ncmd, int remove);
int session_add_cmdq(struct isiSession *ses, struct sescommandset **ncmd);
int session_async_end(struct sescommandset *cmd, int result);

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

/* persistance structs */
struct isiPLoad {
	uint32_t ncid;
	uint64_t uuid;
	struct objtype *obj;
};

/* persistance commands */
int persist_find_session(struct isiSession **ses);
int persist_load_object(uint32_t session, uint32_t cid, uint64_t uuid, uint32_t tid);
int persist_disk(struct isiInfo *info, uint32_t rdblk, uint32_t wrblk, int mode);

/* isi Tx commands */
#define ISIC_TESTLIST 1
#define ISIC_TESTDATA 2
#define ISIC_LOADDESC 10
#define ISIC_LOADOBJECT 11
#define ISIC_LOADRV 12
#define ISIC_LOADNV 13
#define ISIC_LOADNVRANGE 14
#define ISIC_DISKLOAD 15
#define ISIC_DISKWRITE 16
#define ISIC_DISKWRLD 17

/* isi netsync flags */
#define ISIN_SYNC_NONE 0
#define ISIN_SYNC_DEVR 1
#define ISIN_SYNC_DEVV 2
#define ISIN_SYNC_DEVRV 3
#define ISIN_SYNC_MEM 4
#define ISIN_SYNC_MEMDEV 5

/* isi CPU control flags */
#define ISICTL_DEBUG  ( 1 << 0 )
#define ISICTL_STEP   ( 1 << 1 )
#define ISICTL_STEPE  ( 1 << 2 )
#define ISICTL_TRACE  ( 1 << 3 )
#define ISICTL_RUNFOR ( 1 << 4 )
#define ISICTL_TRACEC ( 1 << 5 )

#endif

