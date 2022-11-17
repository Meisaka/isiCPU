#ifndef _ISI_ISITYPES_H_
#define _ISI_ISITYPES_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <new>
#include <string_view>
#include <memory>
#include "reflect.h"
#include "isidefs.h"

#ifdef __cplusplus
#define SUBCLASS(z) : public z
#else
#define SUBCLASS(z)
#endif
class isiInfo;
class isiCPUInfo;
class isiSession;

#if defined(_MSC_VER)
typedef unsigned __int64 SOCKET;
struct fdesc_s {
	union { unsigned __int64 v; void *h; };
	operator SOCKET() { return this->v; };
	fdesc_s & operator=(const SOCKET);
};
typedef fdesc_s fdesc_t;
#else
typedef int fdesc_t;
constexpr fdesc_t fdesc_empty = -1;
#endif

class isiObject {
public:
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

struct isiCommand {
	isiCommand();
	~isiCommand();
	uint32_t cmd;
	uint32_t tid; /* tx ID */
	uint32_t id; /* session ID for tx commands */
	uint32_t param;
	void * rdata; /* alloc/free */
	void * xdata; /* alloc/free */
	void * cptr; /* raw pointer, not freed */
};
typedef uint64_t isi_time_t;

struct net_endpoint;

struct isiMsg {
	isiMsg();
	void reset();
	isiMsg *next;
	uint32_t *buf_alloc;
	union {
		uint32_t *u32_head;
		uint16_t *u16_head;
		uint8_t *u8_head;
	};
	uint8_t *mark;
	union {
		uint8_t *u8;
		uint16_t *u16;
		uint32_t *u32;
	};
	uint32_t length;
	uint32_t limit;
	uint32_t r_limit;
	uint32_t code;
	uint32_t sequence;
	uint32_t txid;
};

struct isiMessageReturn {
	void operator()(isiMsg *) noexcept;
};
typedef std::unique_ptr<isiMsg, isiMessageReturn> isiMsgRef;

class isiSession : public isiObject {
public:
	int stype;
	fdesc_t sfd;
	net_endpoint *r_addr;
	uint32_t recv_index;
	uint32_t frame_length;
	isiMsgRef in;
	void * istate;
	isiCommand *cmdq;
	isiInfo *ccmei;
	uint32_t cmdqstart;
	uint32_t cmdqend;
	uint32_t cmdqlimit;
	int write_msg(isiMsgRef &);
	void multi_write_msg(const isiMsg *);
	virtual int Recv(isi_time_t mtime);
	virtual int STick(isi_time_t mtime);
	virtual int LTick(isi_time_t mtime);
	virtual int AsyncDone(isiCommand *cmd, int result);
	int get_cmdq(isiCommand **ncmd, int remove);
	int add_cmdq(isiCommand **ncmd);
};

struct isiSessionRef {
	uint32_t id;
	uint32_t index;
};

struct isiConPoint {
	isiInfo *t;
	int32_t i;
};

struct isiDevTree {
	struct isiConPoint * table;
	uint32_t count;
	uint32_t limit;
};

class isiNetSync : public isiObject {
public:
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

class isiConstruct : public isiObject {
public:
	uint32_t objtype;
	size_t allocsize;
	const std::string_view name;
	const std::string_view desc;
	struct isiReflection const *rvproto;
	struct isiReflection const *svproto;
	struct isiReflection const *nvproto;
	void const * meta;
	isiConstruct(uint32_t ot, size_t al, const std::string_view na, const std::string_view de) :
		objtype(ot), allocsize(al), name(na), desc(de),
		rvproto(nullptr), svproto(nullptr), nvproto(nullptr), meta(nullptr) {}
	isiConstruct(uint32_t ot, size_t al, const std::string_view na, const std::string_view de,
			isiReflection const *rv, isiReflection const *sv,
			isiReflection const *nv, void const *me) :
		objtype(ot), allocsize(al), name(na), desc(de),
		rvproto(rv), svproto(sv), nvproto(nv), meta(me) {}
	virtual int New(isiObject *o) const { return ISIERR_NOTSUPPORTED; }
};
template<typename T>
class isiClass : public isiConstruct {
public:
	isiClass(uint32_t ot, const std::string_view na, const std::string_view de)
		: isiConstruct(ot, sizeof(T), na, de) {}
	isiClass(uint32_t ot, const std::string_view na, const std::string_view de,
			isiReflection const *rv, isiReflection const *sv,
			isiReflection const *nv, void const *me)
		: isiConstruct(ot, sizeof(T), na, de, rv, sv, nv, me) {}
	virtual int New(isiObject *o) const { new(o)(T)(); return 0; }
};

struct isiMemory;

class isiInfo : public isiObject {
public:
	const isiConstruct * meta;
	isi_time_t nrun;
	void * rvstate;
	void * svstate;
	void * nvstate;
	size_t nvsize;
	struct isiSessionRef sesref;
	struct isiConPoint updev;
	struct isiDevTree busdev;
	isiMemory *mem;

protected:
	int nvalloc(size_t);
public:
	int get_devi(int32_t index, isiInfo **downdev, int32_t *downidx);
	int get_dev(int32_t index, isiInfo **downdev);
public:
	virtual ~isiInfo();
	virtual size_t get_rv_size() const;
	virtual size_t get_sv_size() const;
	virtual size_t get_nv_size() const;
	virtual int Init(const uint8_t *, size_t);
	virtual int Load();
	virtual int Unload();
	virtual int Run(isi_time_t crun);
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int QueryAttach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Attach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Deattach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Reset();
	virtual int on_attached(int32_t to_point, isiInfo *dev, int32_t from_point);
};

class isiDisk : public isiInfo {
public:
	isiDisk();
	virtual int Init(const uint8_t *, size_t);
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int Load();
	virtual int Unload();
	fdesc_t fd;
};
struct isiDiskSeekMsg {
	uint32_t mcode;
	uint32_t ex;
	uint32_t block;
};

class isiCPUInfo : public isiInfo {
public:
	/* CPU specific */
	int ctl;
	size_t rate;
	size_t runrate;
	size_t itvl;
	size_t cycl;
	size_t rcycl;
};

struct isiMemory : public isiObject {
	uint32_t info;
	virtual uint32_t x_rd(uint32_t a) = 0;
	virtual void x_wr(uint32_t a, uint32_t v) = 0;
	virtual uint32_t d_rd(uint32_t a) = 0;
	virtual void d_wr(uint32_t a, uint32_t v) = 0;
	virtual uint32_t i_rd(uint32_t a) const = 0;
	virtual void i_wr(uint32_t a, uint32_t v) = 0;
	virtual uint32_t mask_addr(uint32_t a) const = 0;
	virtual uint32_t mask_data(uint32_t v) const = 0;
	virtual uint32_t byte_offset(uint32_t a) const = 0;
	virtual uint8_t sync_rd(uint32_t a) = 0;
	virtual void sync_set(uint32_t a, uint8_t sv) = 0;
	virtual void sync_clear(uint32_t a, uint8_t sv) = 0;
	virtual void sync_wrblock(uint32_t a, uint32_t l, const uint8_t *src) = 0;
	virtual void sync_rdblock(uint32_t a, uint32_t l, uint8_t *dst) = 0;
	virtual bool isbrk(uint32_t a) const = 0;
	virtual void togglebrk(uint32_t a) = 0;
};

#define ISI_RAMCTL_DELTA (1<<0)
#define ISI_RAMCTL_RDONLY (1<<1)
#define ISI_RAMCTL_SYNC (1<<2)
#define ISI_RAMCTL_BREAK (1<<7)
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
int isi_find_cpu(uint32_t id, isiInfo **target, size_t *index);
int isi_message_dev(isiInfo *src, int32_t srcindex, uint32_t *wordbuf, uint32_t words, isi_time_t mtime);
uint32_t isi_lookup_name(const std::string_view);
int isi_get_name(uint32_t cid, std::string_view *name);
int isi_write_parameter(uint8_t *p, size_t plen, int code, const void *in, size_t limit);
int isi_fetch_parameter(const uint8_t *p, size_t plen, int code, void *out, size_t limit);
void * isi_realloc(void *h, size_t mem);
void * isi_calloc(size_t mem);
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
void isilogneterr(const char * desc);
void isilogerr(const char * desc);
void isilog(int level, const char *format, ...);

int isi_register(isiConstruct *obj);

int loadbinfile(const char* path, int endian, unsigned char **nmem, uint32_t *nsize);
int loadbinfileto(const char* path, int endian, unsigned char *nmem, uint32_t nsize);
int savebinfile(const char* path, int endian, unsigned char *nmem, uint32_t nsize);
int isi_text_dec(const char *text, size_t len, size_t limit, void *vv, int olen);
int isi_disk_getblock(isiInfo *disk, void **blockaddr);
int isi_disk_getindex(isiInfo *disk, uint32_t *blockindex);
int isi_disk_isreadonly(isiInfo *disk);
int isi_find_bin(uint64_t diskid, char **nameout);
size_t isi_fsize(const char *path);
int isi_fname_id(const char *fname, uint64_t *id);

isiMsgRef make_msg();
void session_multicast_msg(isiMsgRef &);
int session_async_end(isiCommand *cmd, int result);

void isi_debug_dump_synctable();
int isi_add_memsync(isiObject *target, uint32_t base, uint32_t extent, size_t rate);
int isi_add_sync_extent(isiObject *target, uint32_t base, uint32_t extent);
int isi_set_sync_extent(isiObject *target, uint32_t index, uint32_t base, uint32_t extent);
int isi_add_devsync(isiObject *target, size_t rate);
int isi_add_devmemsync(isiObject *target, isiObject *memtarget, size_t rate);
int isi_set_devmemsync_extent(isiObject *target, isiObject *memtarget, uint32_t index, uint32_t base, uint32_t extent);
int isi_resync_dev(isiObject *target);
int isi_resync_all();
int isi_remove_sync(isiObject *target);

/* persistance structs */
struct isiPLoad {
	uint32_t ncid;
	uint64_t uuid;
	isiObject *obj;
};

/* persistance commands */
int persist_find_session(isiSession **ses);
int persist_load_object(uint32_t session, uint32_t cid, uint64_t uuid, uint32_t tid);
int persist_disk(isiInfo *info, uint32_t rdblk, uint32_t wrblk, int mode);

/* isi Tx commands */
constexpr int ISIC_TESTLIST = 1;
constexpr int ISIC_TESTDATA = 2;
constexpr int ISIC_LOADDESC = 10;
constexpr int ISIC_LOADOBJECT = 11;
constexpr int ISIC_LOADRV = 12;
constexpr int ISIC_LOADNV = 13;
constexpr int ISIC_LOADNVRANGE = 14;
constexpr int ISIC_DISKLOAD = 15;
constexpr int ISIC_DISKWRITE = 16;
constexpr int ISIC_DISKWRLD = 17;

/* isi netsync flags */
constexpr int ISIN_SYNC_NONE = 0;
constexpr int ISIN_SYNC_DEVR = 1;
constexpr int ISIN_SYNC_DEVV = 2;
constexpr int ISIN_SYNC_DEVRV = 3;
constexpr int ISIN_SYNC_MEM = 4;
constexpr int ISIN_SYNC_MEMDEV = 5;

/* isi CPU control flags */
constexpr int ISICTL_DEBUG  = (1 << 0);
constexpr int ISICTL_STEP   = (1 << 1);
constexpr int ISICTL_STEPE  = (1 << 2);
constexpr int ISICTL_TRACE  = (1 << 3);
constexpr int ISICTL_RUNFOR = (1 << 4);
constexpr int ISICTL_TRACEC = (1 << 5);

#endif

