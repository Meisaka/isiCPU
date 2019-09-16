#ifndef _ISI_ISITYPES_H_
#define _ISI_ISITYPES_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/time.h>
#include <new>
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

struct isiObject {
	uint32_t otype;
	uint32_t id;
	uint64_t uuid;
	virtual ~isiObject() {}
};

struct isiObjSlot {
	isiObject * ptr;
	uint32_t href;
	uint32_t sref;
};

struct isiObjRef {
	uint64_t uuid;
	uint32_t id;
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
typedef uint64_t isi_time_t;

struct isiSession : public isiObject {
	int stype;
	int sfd;
	struct sockaddr_in r_addr;
	uint32_t rcv;
	uint8_t *in;
	uint8_t *out;
	void * istate;
	isi_time_t rqtimeout;
	struct sescommandset *cmdq;
	struct isiInfo *ccmei;
	uint32_t cmdqstart;
	uint32_t cmdqend;
	uint32_t cmdqlimit;
	virtual int Recv(isi_time_t mtime);
	virtual int STick(isi_time_t mtime);
	virtual int LTick(isi_time_t mtime);
	virtual int AsyncDone(struct sescommandset *cmd, int result);
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
	struct isiObjSlot * table;
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

struct isiNetSync : public isiObject {
	isiObject target;
	isiObject memobj;
	int synctype;
	int ctl;
	int target_index;
	int memobj_index;
	size_t rate;
	isi_time_t nrun;
	uint32_t extents;
	uint32_t base[4];
	uint32_t len[4];
};

struct isiConstruct : public isiObject {
	uint32_t objtype;
	size_t allocsize;
	const char * name;
	const char * desc;
	struct isiReflection const *rvproto;
	struct isiReflection const *svproto;
	struct isiReflection const *nvproto;
	void const * meta;
	isiConstruct(uint32_t ot, size_t al, const char *na, const char *de) :
		objtype(ot), allocsize(al), name(na), desc(de),
		rvproto(nullptr), svproto(nullptr), nvproto(nullptr), meta(nullptr) {}
	isiConstruct(uint32_t ot, size_t al, const char *na, const char *de,
			isiReflection const *rv, isiReflection const *sv,
			isiReflection const *nv, void const *me) :
		objtype(ot), allocsize(al), name(na), desc(de),
		rvproto(rv), svproto(sv), nvproto(nv), meta(me) {}
	virtual int New(struct isiObject *o) const { return ISIERR_NOTSUPPORTED; }
};
template<typename T>
struct isiClass : public isiConstruct {
	isiClass(uint32_t ot, const char *na, const char *de)
		: isiConstruct(ot, sizeof(T), na, de) {}
	isiClass(uint32_t ot, const char *na, const char *de,
			isiReflection const *rv, isiReflection const *sv,
			isiReflection const *nv, void const *me)
		: isiConstruct(ot, sizeof(T), na, de, rv, sv, nv, me) {}
	virtual int New(struct isiObject *o) const { new(o)(T)(); return 0; }
};

struct isiConTable {
	struct isiConstruct const ** table;
	uint32_t count;
	uint32_t limit;
};

struct isiRam;

class isiInfo : public isiObject {
public:
	const struct isiConstruct * meta;
	isi_time_t nrun;
	void * rvstate;
	void * svstate;
	void * nvstate;
	size_t nvsize;
	struct isiSessionRef sesref;
	struct isiConPoint updev;
	struct isiDevTree busdev;
	isiRam *mem;

protected:
	int nvalloc(size_t);
public:
	int get_devi(int32_t index, isiInfo **downdev, int32_t *downidx);
	int get_dev(int32_t index, isiInfo **downdev);
public:
	virtual ~isiInfo();
	virtual int Init(const uint8_t *, size_t);
	virtual int QuerySize(int, size_t *) const;
	virtual int Load();
	virtual int Unload();
	virtual int Run(isi_time_t crun);
	virtual int MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int QueryAttach(int32_t topoint, struct isiInfo *dev, int32_t frompoint);
	virtual int Attach(int32_t topoint, struct isiInfo *dev, int32_t frompoint);
	virtual int Attached(int32_t topoint, struct isiInfo *dev, int32_t frompoint);
	virtual int Deattach(int32_t topoint, struct isiInfo *dev, int32_t frompoint);
	virtual int Reset();
};

struct isiDisk : public isiInfo {
	isiDisk();
	virtual int Init(const uint8_t *, size_t);
	virtual int MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int Load();
	virtual int Unload();
	int fd;
};
struct isiDiskSeekMsg {
	uint32_t mcode;
	uint32_t ex;
	uint32_t block;
};

struct isiCPUInfo : public isiInfo {
	/* CPU specific */
	int ctl;
	size_t rate;
	size_t runrate;
	size_t itvl;
	size_t cycl;
	size_t rcycl;
};

struct isiRam : public isiObject {
	virtual uint32_t x_rd(uint32_t a);
	virtual void x_wr(uint32_t a, uint32_t v);
	virtual uint32_t d_rd(uint32_t a);
	virtual void d_wr(uint32_t a, uint32_t v);
	virtual int isbrk(uint32_t a);
	virtual void togglebrk(uint32_t a);
};
typedef struct isiRam *isiram;

struct memory64x16 : public isiRam {
	uint16_t ram[0x10000];
	uint32_t ctl[0x10000];
	uint32_t info;
	virtual uint32_t x_rd(uint32_t a);
	virtual void x_wr(uint32_t a, uint32_t v);
	virtual uint32_t d_rd(uint32_t a);
	virtual void d_wr(uint32_t a, uint32_t v);
	virtual int isbrk(uint32_t a);
	virtual void togglebrk(uint32_t a);
};

#define ISI_RAMCTL_DELTA (1<<16)
#define ISI_RAMCTL_SYNC (1<<17)
#define ISI_RAMCTL_RDONLY (1<<18)
#define ISI_RAMCTL_BREAK (1<<19)
#define ISI_RAMINFO_SCAN 1

/* boolean functions */
int isi_is_memory(isiObject const *item);
int isi_is_cpu(isiObject const *item);
int isi_is_bus(isiObject const *item);
int isi_is_infodev(isiObject const *item);

/* attach dev to item */
int isi_attach(isiInfo *item, int32_t itempoint, isiInfo *dev, int32_t devpoint, int32_t *itemactual_out, int32_t *devactual_out);
int isi_deattach(isiInfo *item, int32_t itempoint);
int isi_make_object(uint32_t objtype, isiObject **out, const uint8_t *cfg, size_t lcfg);
int isi_create_object(uint32_t objtype, isiConstruct const **outcon, isiObject **out);
int isi_delete_object(isiObject *obj);
int isi_find_obj(uint32_t id, isiObject **target);
int isi_find_uuid(uint32_t cid, uint64_t uuid, isiObject **target);
int isi_createdev(isiInfo **ndev);
int isi_push_dev(isiDevTable *t, isiInfo *d);
int isi_pop_dev(isiDevTable *t, isiInfo *d);
int isi_find_dev(isiDevTable *t, uint32_t id, isiInfo **target, size_t *index);
int isi_message_dev(isiInfo *src, int32_t srcindex, uint32_t *, int, isi_time_t mtime);
uint32_t isi_lookup_name(const char *);
int isi_get_name(uint32_t cid, const char **name);
int isi_write_parameter(uint8_t *p, int plen, int code, const void *in, int limit);
int isi_fetch_parameter(const uint8_t *p, int plen, int code, void *out, int limit);
void * isi_realloc(void *h, size_t mem);
void * isi_alloc(size_t mem);
void * isi_alloc_array(size_t count, size_t elemsize);
int isi_text_enc(char *text, int limit, void const *vv, int len);

void isi_fetch_time(isi_time_t * t);
void isi_add_time(isi_time_t *, size_t nsec);
int isi_time_lt(isi_time_t const *, isi_time_t const *);
void isi_setrate(isiCPUInfo *info, size_t rate);

#define L_DEBUG 5
#define L_NOTE 4
#define L_INFO 3
#define L_WARN 2
#define L_ERR 1
#define L_CRIT 0
void isilogerr(const char * desc);
void isilog(int level, const char *format, ...);

int isi_register(isiConstruct *obj);
int isi_inittable(isiDevTable *t);

int loadbinfile(const char* path, int endian, unsigned char **nmem, uint32_t *nsize);
int loadbinfileto(const char* path, int endian, unsigned char *nmem, uint32_t nsize);
int savebinfile(const char* path, int endian, unsigned char *nmem, uint32_t nsize);
int isi_text_dec(const char *text, int len, int limit, void *vv, int olen);
int isi_disk_getblock(isiInfo *disk, void **blockaddr);
int isi_disk_getindex(isiInfo *disk, uint32_t *blockindex);
int isi_disk_isreadonly(isiInfo *disk);
int isi_find_bin(uint64_t diskid, char **nameout);
size_t isi_fsize(const char *path);
int isi_fname_id(const char *fname, uint64_t *id);
int isi_text_dec(const char *text, int len, int limit, void *vv, int olen);

int session_write(isiSession *, int len);
int session_write_msg(isiSession *);
int session_write_msgex(isiSession *, void *);
int session_write_buf(isiSession *, void *, int len);
int isi_pushses(isiSession *s);
int session_get_cmdq(isiSession *ses, struct sescommandset **ncmd, int remove);
int session_add_cmdq(isiSession *ses, struct sescommandset **ncmd);
int session_async_end(struct sescommandset *cmd, int result);

void isi_synctable_init();
void isi_debug_dump_synctable();
int isi_add_memsync(isiObject *target, uint32_t base, uint32_t extent, size_t rate);
int isi_add_sync_extent(isiObject *target, uint32_t base, uint32_t extent);
int isi_set_sync_extent(isiObject *target, uint32_t index, uint32_t base, uint32_t extent);
int isi_add_devsync(isiObject *target, size_t rate);
int isi_add_devmemsync(isiObject *target, struct isiObject *memtarget, size_t rate);
int isi_set_devmemsync_extent(isiObject *target, isiObject *memtarget, uint32_t index, uint32_t base, uint32_t extent);
int isi_resync_dev(isiObject *target);
int isi_resync_all();
int isi_remove_sync(isiObject *target);

/* persistance structs */
struct isiPLoad {
	uint32_t ncid;
	uint64_t uuid;
	struct isiObject *obj;
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

