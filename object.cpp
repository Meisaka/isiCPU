
#include "isitypes.h"

struct isiDevTable alldev;
struct isiDevTable allcpu;
struct isiConTable allcon;
struct isiObjTable allobj;

static uint32_t maxsid = 0;

static int isi_insertindex_dev(struct isiInfo *dev, int32_t index, struct isiInfo *downdev, int32_t rindex);
static int isi_appendindex_dev(struct isiInfo *dev, struct isiInfo *downdev, int32_t *downindexout, int32_t rindex);
static int isi_setindex_dev(struct isiInfo *dev, uint32_t index, struct isiInfo *downdev, int32_t downindex);
int isi_message_ses(struct isiSessionRef *sr, uint32_t oid, uint32_t *msg, int len);

int isi_write_parameter(uint8_t *p, int plen, int code, const void *in, int limit)
{
	if(!p || plen < 1 || !code) return -1;
	int found = 0;
	int flen = 0;
	uint8_t *uo = (uint8_t*)in;
	for(int i = plen; i--; p++) {
		switch(found) {
		case 0:
			if(*p == 0) {
				found = 1;
				*p = code;
			}
			else found = 2;
			break;
		case 1:
			flen = *p = (uint8_t)limit;
			if(flen) found += 2;
			else return 0;
			break;
		case 2:
			flen = *p;
			if(flen) found += 2;
			else found = 0;
			break;
		case 3:
			if(limit && flen) {
				*p = *uo;
				uo++;
				limit--;
				flen--;
				if(!limit && !i) return 0;
			} else {
				*p = 0;
				return 0;
			}
			break;
		case 4:
			flen--;
			if(!flen) found = 0;
			break;
		default:
			found = 0;
			break;
		}
	}
	return 1;
}

int isi_fetch_parameter(const uint8_t *p, int plen, int code, void *out, int limit)
{
	if(!p || plen < 1 || !code) return -1;
	int found = 0;
	int flen = 0;
	uint8_t *uo = (uint8_t*)out;
	for(int i = plen; i--; p++) {
		switch(found) {
		case 0:
			if((*p) == code) found = 1;
			else found = 2;
			if(!*p) return 1;
			break;
		case 1:
			flen = *p;
			if(flen) found += 2;
			else return 0;
			break;
		case 2:
			flen = *p;
			if(flen) found += 2;
			else found = 0;
			break;
		case 3:
			if(limit && flen) {
				*uo = *p;
				uo++;
				limit--;
				flen--;
				if(!limit) return 0;
			} else {
				return 0;
			}
			break;
		case 4:
			flen--;
			if(!flen) found = 0;
			break;
		default:
			found = 0;
			break;
		}
	}
	return 1;
}

void * isi_realloc(void *h, size_t mem)
{
	void * p;
	if(h) {
		p = realloc(h, mem);
		if(!p) return NULL;
	} else {
		p = malloc(mem);
		if(!p) return NULL;
		memset(p, 0, mem);
	}
	return p;
}

void * isi_alloc(size_t mem)
{
	void * p;
	p = malloc(mem);
	if(!p) return NULL;
	memset(p, 0, mem);
	return p;
}

static void isi_update_busmem(struct isiInfo *bus, struct isiRam *mem)
{
	size_t k;
	size_t hs;
	hs = bus->busdev.count;
	for(k = 0; k < hs; k++) {
		if(bus->busdev.table[k].t) {
			bus->busdev.table[k].t->mem = mem;
		}
	}
}

int isi_is_memory(isiObject const *item)
{
	return ISIT_IS(item->otype, ISIT_MEMORY);
}
int isi_is_cpu(isiObject const *item)
{
	return ISIT_IS(item->otype, ISIT_CPU);
}
int isi_is_bus(isiObject const *item)
{
	return ISIT_IS(item->otype, ISIT_BUSDEV);
}
int isi_is_infodev(isiObject const *item)
{
	return ISIT_IS_INFO(item->otype);
}

int isi_message_dev(struct isiInfo *src, int32_t srcindex, uint32_t *m, int l, isi_time_t mtime)
{
	if(!src || srcindex < ISIAT_LIMIT) return -1;
	struct isiInfo *dest = NULL;
	int32_t lsi = srcindex;
	if(srcindex == ISIAT_UP) {
		dest = src->updev.t;
		lsi = src->updev.i;
	} else if(srcindex == ISIAT_SESSION) {
		return isi_message_ses(&src->sesref, src->id, m, l);
	} else if(srcindex >= 0) {
		src->get_devi(srcindex, &dest, &lsi);
	} else if(srcindex == -1) {
		dest = src;
	} else return -1;
	if(!dest) return -1;
	return dest->MsgIn(src, lsi, m, l, mtime);
}

int isi_deattach(struct isiInfo *item, int32_t itempoint)
{
	if(!item || itempoint < ISIAT_LIMIT || itempoint == ISIAT_APPEND) return ISIERR_INVALIDPARAM;
	if(item->otype < 0x2f00) return ISIERR_INVALIDPARAM; /* can't process weird things */
	struct isiInfo *dev = NULL;
	if(itempoint == ISIAT_UP) {
		dev = item->updev.t;
	} else if(itempoint == ISIAT_INSERTSTART) {
		return ISIERR_NOTSUPPORTED;
	} else if(item->get_dev(itempoint, &dev)) {
		return ISIERR_FAIL;
	}
	int32_t rai;
	if(itempoint == ISIAT_UP) {
		rai = item->updev.i;
		item->updev.t = NULL;
		item->updev.i = -1;
	} else if(itempoint >= 0) {
		rai = item->busdev.table[itempoint].i; /* getindex_dev should make sure this is valid */
		item->busdev.table[itempoint].t = NULL;
		item->busdev.table[itempoint].i = -1;
		/* collapse empty space at end of table */
		for(int i = item->busdev.count; i-->0 && !item->busdev.table[i].t; item->busdev.count--);
	} else {
		isilog(L_WARN, "invalid local deattach point A=%d", itempoint);
		rai = -1;
	}
	isilog(L_DEBUG, "deattach call A=%d B=%d\n", itempoint, rai);
	if(dev->otype < 0x2f00) return 0; /* weird thing, we're done */
	if(rai == ISIAT_UP) {
		dev->updev.t = NULL;
		dev->updev.i = -1;
	} else if(rai >= 0 && !dev->get_dev(rai, NULL)) {
		dev->busdev.table[rai].t = NULL;
		dev->busdev.table[rai].i = -1;
		/* collapse empty space at end of table */
		for(int i = dev->busdev.count; i-->0 && !dev->busdev.table[i].t; dev->busdev.count--);
	} else {
		isilog(L_WARN, "invalid remote deattach point A=%d B=%d", itempoint, rai);
	}
	return 0;
}

int isi_attach(struct isiInfo *item, int32_t itempoint, struct isiInfo *dev, int32_t devpoint, int32_t *itemactual_out, int32_t *devactual_out)
{
	int e = 0;
	if(!item || !dev) return ISIERR_INVALIDPARAM;
	if(itempoint < ISIAT_LIMIT || devpoint < ISIAT_LIMIT) return ISIERR_INVALIDPARAM;
	if(itempoint == ISIAT_SESSION || devpoint == ISIAT_SESSION) return ISIERR_INVALIDPARAM;
	if(!isi_is_infodev(item)) return ISIERR_INVALIDPARAM; /* can't process weird things */
	if(isi_is_memory(dev)) { /* handle attaching memory */
		if((e = item->QueryAttach(itempoint, dev, devpoint))) {
			return e;
		}
		item->mem = (isiRam*)dev;
		if(isi_is_cpu(item)) item->Attach(itempoint, dev, devpoint);
		if(isi_is_bus(item)) isi_update_busmem(item, item->mem);
		return 0;
	}
	int skipq = 0;
	if(!isi_is_infodev(dev)) return ISIERR_NOCOMPAT; /* can't attach weird things to other things */
	if(!skipq) {
		if((e = item->QueryAttach(itempoint, dev, devpoint))) {
			return e;
		}
	}
	// else if(!isi_is_bus(item)) return ISIERR_NOCOMPAT;
	/* TODO this area might have to be redone to support interface types, etc */
	if(!skipq) {
		if((e = dev->QueryAttach(devpoint, item, itempoint))) {
			return e; /* test the other device too */
		}
	}
	if(itempoint >= 0) {
		if(!item->get_dev(itempoint, 0)) {
			return ISIERR_ATTACHINUSE;
		}
	}
	if(devpoint >= 0) {
		if(!dev->get_dev(devpoint, 0)) {
			return ISIERR_ATTACHINUSE;
		}
	}
	int32_t iout = 0;
	int32_t *iptr = 0;
	if(devpoint == ISIAT_APPEND) {
		isi_appendindex_dev(dev, item, &iout, itempoint);
		iptr = &dev->busdev.table[iout].i;
		devpoint = iout;
		if(devactual_out) *devactual_out = iout;
	} else if(devpoint == ISIAT_UP) {
		dev->updev.t = item;
		dev->updev.i = itempoint;
		iptr = &dev->updev.i;
		if(devactual_out) *devactual_out = ISIAT_UP;
		if(item->mem) {
			dev->mem = item->mem;
			if(isi_is_bus(dev)) isi_update_busmem(dev, item->mem);
		}
	} else if(devpoint == ISIAT_INSERTSTART) {
		isi_insertindex_dev(dev, 0, item, itempoint);
		iptr = &dev->busdev.table[0].i;
		devpoint = 0;
		if(devactual_out) *devactual_out = 0;
	} else {
		isi_setindex_dev(dev, devpoint, item, itempoint);
		if(devactual_out) *devactual_out = devpoint;
	}
	if(itempoint == ISIAT_APPEND) {
		isi_appendindex_dev(item, dev, &iout, devpoint);
		if(iptr) *iptr = iout;
		if(itemactual_out) *itemactual_out = iout;
	} else if(itempoint == ISIAT_UP) {
		item->updev.t = dev;
		item->updev.i = devpoint;
		if(itemactual_out) *itemactual_out = ISIAT_UP;
	} else if(itempoint == ISIAT_INSERTSTART) {
		isi_insertindex_dev(item, 0, dev, devpoint);
		if(itemactual_out) *itemactual_out = 0;
	} else {
		isi_setindex_dev(item, itempoint, dev, devpoint);
		if(itemactual_out) *itemactual_out = itempoint;
	}
	item->Attach(itempoint, dev, devpoint);
	dev->Attached(devpoint, item, itempoint);
	return 0;
}

void isi_objtable_init()
{
	allobj.limit = 256;
	allobj.count = 0;
	allobj.table = (isiObjSlot*)isi_alloc(allobj.limit * sizeof(isiObjSlot));
}

int isi_objtable_add(isiObject *obj)
{
	if(!obj) return -1;
	void *n;
	if(allobj.count >= allobj.limit) {
		n = isi_realloc(allobj.table, (allobj.limit + allobj.limit) * sizeof(isiObjSlot));
		if(!n) return -5;
		allobj.limit += allobj.limit;
		allobj.table = (isiObjSlot*)n;
	}
	allobj.table[allobj.count++].ptr = obj;
	return 0;
}

int isi_find_obj(uint32_t id, isiObject **target)
{
	size_t i;
	if(!id) return -1;
	for(i = 0; i < allobj.count; i++) {
		isiObject *obj = allobj.table[i].ptr;
		if(obj && obj->id == id) {
			if(target) {
				*target = obj;
			}
			return 0;
		}
	}
	return 1;
}

int isi_find_uuid(uint32_t cid, uint64_t uuid, isiObject **target)
{
	size_t i;
	if(!cid || !uuid) return -1;
	for(i = 0; i < allobj.count; i++) {
		isiObject *obj = allobj.table[i].ptr;
		if(obj && obj->otype == cid && obj->uuid == uuid) {
			if(target) {
				*target = obj;
			}
			return 0;
		}
	}
	return 1;
}

int isi_create_object(uint32_t objtype, isiConstruct const **outcon, isiObject **out)
{
	uint32_t x;
	int rs = 0;
	struct isiConstruct const *con = NULL;
	if(!objtype || !out) return ISIERR_INVALIDPARAM;
	for(x = 0; x < allcon.count; x++) {
		if(allcon.table[x]->objtype == objtype) {
			con = allcon.table[x];
			break;
		}
	}
	if(outcon) *outcon = con;
	if(!con) return ISIERR_NOTFOUND;
	isiObject *ndev;
	size_t objsize = con->allocsize;
	if(!objsize) return ISIERR_INVALIDPARAM;
	ndev = (isiObject*)isi_alloc(objsize);
	if(!ndev) return ISIERR_NOMEM;
	isi_objtable_add(ndev);
	ndev->otype = objtype;
	if((rs = con->New(ndev))) {
		isi_delete_object(ndev);
		*out = 0;
		return rs;
	}
	ndev->otype = objtype;
	ndev->id = ++maxsid; // TODO make "better" ID numbers?
	if(!ISIT_IS_INFO(objtype)) {
		*out = ndev;
		return 0;
	}
	struct isiInfo *info = (struct isiInfo*)ndev;
	info->meta = con;
	size_t sz = 0;
	info->QuerySize(0, &sz);
	if(sz) {
		info->rvstate = isi_alloc(sz);
	} else if(con->rvproto && con->rvproto->length) {
		info->rvstate = isi_alloc(con->rvproto->length);
	}
	sz = 0;
	info->QuerySize(1, &sz);
	if(sz) {
		info->svstate = isi_alloc(sz);
	} else if(con->svproto && con->svproto->length) {
		info->svstate = isi_alloc(con->svproto->length);
	}
	*out = ndev;
	return 0;
}

int isi_make_object(uint32_t objtype, isiObject **out, const uint8_t *cfg, size_t lcfg)
{
	int rs = 0;
	isiObject *ndev;
	struct isiConstruct const *con = NULL;
	rs = isi_create_object(objtype, &con, &ndev);
	if(rs) return rs;
	if(!ISIT_IS_INFO(objtype)) {
		*out = ndev;
		return 0;
	}
	isiInfo *info = (isiInfo*)ndev;
	rs = info->Init(cfg, lcfg);
	if(rs) {
		isi_delete_object(ndev);
		*out = 0;
		return rs;
	}
	info->Load();
	*out = ndev;
	return 0;
}

int isi_delete_object(isiObject *obj)
{
	if(!obj) return ISIERR_INVALIDPARAM;
	struct isiObjTable *t = &allobj;
	uint32_t i;
	for(i = 0; i < t->count; i++) {
		if(t->table[i].ptr == obj) {
			t->table[i].ptr = NULL;
			break;
		}
	}
	if(i >= t->limit) return -1;
	if(ISIT_IS_INFO(obj->otype)) {
		isiInfo *info = (isiInfo *)obj;
		info->Unload();
		info->~isiObject();
		if(info->rvstate) {
			free(info->rvstate);
		}
		if(info->svstate) {
			free(info->svstate);
		}
		if(info->nvstate) {
			free(info->nvstate);
		}
		memset(info, 0, info->meta->allocsize);
	} else {
		obj->~isiObject();
		memset(obj, 0, sizeof(isiObject));
	}
	free(obj);
	return 0;
}

int isi_init_contable()
{
	struct isiConTable *t = &allcon;
	t->limit = 32;
	t->count = 0;
	t->table = (struct isiConstruct const**)isi_alloc(t->limit * sizeof(void*));
	return 0;
}

int isi_contable_add(struct isiConstruct const *obj)
{
	if(!obj) return -1;
	void *n;
	if(allcon.count >= allcon.limit) {
		n = isi_realloc(allcon.table, (allcon.limit + allcon.limit) * sizeof(void*));
		if(!n) return -5;
		allcon.limit += allcon.limit;
		allcon.table = (struct isiConstruct const**)n;
	}
	allcon.table[allcon.count++] = obj;
	return 0;
}

int isi_register(struct isiConstruct *obj)
{
	int itype;
	int inum;
	if(!obj) return -1;
	if(!obj->name) return -1;
	if(!obj->objtype) return -1;
	itype = obj->objtype & ISIT_CLASS_MASK;
	inum = obj->objtype & ISIT_INUM_MASK;
	for(uint32_t i = 0; i < allcon.count; i++) {
		int ltype = allcon.table[i]->objtype & ISIT_CLASS_MASK;
		int lnum = allcon.table[i]->objtype & ISIT_INUM_MASK;
		if(ltype == itype) {
			if(lnum >= inum) {
				inum = lnum + 1;
			}
		}
	}
	obj->objtype = (itype & ISIT_CLASS_MASK) | (inum & ISIT_INUM_MASK);
	isi_contable_add(obj);
	isilog(L_INFO, "class: %08x %s registered\n", obj->objtype, obj->name);
	return 0;
}

uint32_t isi_lookup_name(const char * name)
{
	if(!name) return 0;
	for(uint32_t i = 0; i < allcon.count; i++) {
		if(!strcmp(allcon.table[i]->name, name)) {
			return allcon.table[i]->objtype;
		}
	}
	isilog(L_INFO, "object: lookup name failed for: %s\n", name);
	return 0;
}

int isi_get_name(uint32_t cid, const char **name)
{
	if(!name) return -1;
	for(uint32_t i = 0; i < allcon.count; i++) {
		if(allcon.table[i]->objtype == cid) {
			*name = allcon.table[i]->name;
			return 0;
		}
	}
	return ISIERR_NOTFOUND;
}

static int isi_initindex_dev(struct isiInfo *item)
{
	item->busdev.limit = 1;
	item->busdev.count = 0;
	item->busdev.table = (struct isiConPoint*)isi_alloc(item->busdev.limit * sizeof(struct isiConPoint));
	return 0;
}

static int isi_indexresize(struct isiInfo *item, size_t size)
{
	if(size <= item->busdev.limit) return 0;
	void *n;
	n = isi_realloc(item->busdev.table, size * sizeof(struct isiConPoint));
	if(!n) return -5;
	item->busdev.limit = size;
	item->busdev.table = (struct isiConPoint*)n;
	return 0;
}

static int isi_insertindex_dev(struct isiInfo *dev, int32_t index, struct isiInfo *downdev, int32_t rindex)
{
	return ISIERR_NOTSUPPORTED;
}

static int isi_appendindex_dev(struct isiInfo *item, struct isiInfo *d, int32_t *dindex, int32_t rindex)
{
	if(!item) return -1;
	if(!item->busdev.limit || !item->busdev.table) isi_initindex_dev(item);
	if(item->busdev.count >= item->busdev.limit) {
		int r;
		r = isi_indexresize(item, item->busdev.limit + item->busdev.limit);
		if(r) return r;
	}
	int32_t x = item->busdev.count;
	item->busdev.count++;
	item->busdev.table[x].t = d;
	item->busdev.table[x].i = rindex;
	if(dindex) *dindex = x;
	return 0;
}

int isiInfo::nvalloc(size_t sz) {
	void *ac = isi_alloc(sz);
	if(ac) {
		this->nvstate = ac;
		this->nvsize = sz;
		return 0;
	}
	return ISIERR_NOMEM;
}

int isiInfo::get_devi(int32_t index, struct isiInfo **downdev, int32_t *downidx)
{
	if(index >= this->busdev.count || index >= this->busdev.limit) return -1;
	if(!this->busdev.table) return -1;
	struct isiInfo *o;
	int32_t ix;
	if(index == ISIAT_UP) o = this->updev.t;
	else if(index == ISIAT_APPEND) {
		if(this->busdev.count < 1) return -1;
		o = this->busdev.table[this->busdev.count - 1].t;
		ix = this->busdev.table[this->busdev.count - 1].i;
	} else if(index < 0) {
		return -1;
	} else {
		o = this->busdev.table[index].t;
		ix = this->busdev.table[index].i;
	}
	if(!o) return -1;
	if(downdev) *downdev = o;
	if(downidx) *downidx = ix;
	return 0;
}
int isiInfo::get_dev(int32_t index, struct isiInfo **downdev)
{
	return this->get_devi(index, downdev, 0);
}
isiInfo::~isiInfo() {}
int isiInfo::Init(const uint8_t *, size_t)
{
	return 0;
}
int isiInfo::QuerySize(int, size_t *sz) const
{
	if(sz) *sz = 0;
	return 0;
}
int isiInfo::Load()
{
	return 0;
}
int isiInfo::Unload()
{
	return 0;
}
int isiInfo::Run(isi_time_t crun)
{
	return 1;
}
int isiInfo::MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	return 1;
}
int isiInfo::QueryAttach(int32_t topoint, struct isiInfo *dev, int32_t frompoint)
{
	return 0;
}
int isiInfo::Attach(int32_t topoint, struct isiInfo *dev, int32_t frompoint)
{
	return 0;
}
int isiInfo::Attached(int32_t topoint, struct isiInfo *dev, int32_t frompoint)
{
	return 0;
}
int isiInfo::Deattach(int32_t topoint, struct isiInfo *dev, int32_t frompoint)
{
	return 0;
}
int isiInfo::Reset() { return 0; }

static int isi_setindex_dev(struct isiInfo *dev, uint32_t index, struct isiInfo *downdev, int32_t downindex)
{
	if(!dev) return -1;
	if(!dev->busdev.limit || !dev->busdev.table) isi_initindex_dev(dev);
	if(index >= dev->busdev.limit) {
		int r;
		r = isi_indexresize(dev, index + 1);
		if(r) return r;
	}
	if(dev->busdev.count < index + 1) dev->busdev.count = index + 1;
	dev->busdev.table[index].t = downdev;
	dev->busdev.table[index].i = downindex;
	return 0;
}

static int isi_tableresize(struct isiDevTable *t, size_t size)
{
	if(size <= t->limit) return 0;
	void *n;
	n = isi_realloc(t->table, (size) * sizeof(void*));
	if(!n) return -5;
	t->limit = size;
	t->table = (struct isiInfo**)n;
	return 0;
}

int isi_inittable(struct isiDevTable *t)
{
	t->limit = 32;
	t->count = 0;
	t->table = (struct isiInfo**)isi_alloc(t->limit * sizeof(void*));
	return 0;
}

int isi_push_dev(struct isiDevTable *t, isiInfo *d)
{
	if(!d) return -1;
	if(!t->limit || !t->table) isi_inittable(t);
	if(t->count >= t->limit) {
		int r;
		r = isi_tableresize(t, t->limit + t->limit);
		if(r) return r;
	}
	t->table[t->count++] = d;
	return 0;
}

int isi_pop_dev(struct isiDevTable *t, isiInfo *d)
{
	if(!d) return -1;
	if(!t->limit || !t->table) isi_inittable(t);
	if(!t->count) return -1;
	size_t index = 0;
	uint32_t id = d->id;
	while(index < t->count) { /* find the object */
		isiInfo *obj = t->table[index];
		if(obj && obj->id == id) break;
		index++;
	}
	if(index >= t->count) return -1;
	t->count--;
	do { /* shift everything down */
		t->table[index] = t->table[index + 1];
		index++;
	} while(index < t->count);
	return 0;
}

int isi_find_dev(struct isiDevTable *t, uint32_t id, isiInfo **target, size_t *index)
{
	size_t i;
	if(!id) return -1;
	for(i = 0; i < t->count; i++) {
		struct isiInfo *obj = t->table[i];
		if(obj && obj->id == id) {
			if(target) *target = obj;
			if(index) *index = i;
			return 0;
		}
	}
	return 1;
}

int isi_createdev(isiInfo **ndev)
{
	return isi_create_object(ISIT_HARDWARE, NULL, (isiObject**)ndev);
}

int isi_createcpu(isiCPUInfo **ndev)
{
	return isi_create_object(ISIT_CPU, NULL, (isiObject**)ndev);
}


