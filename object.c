
#include "cputypes.h"

struct isiDevTable alldev;
struct isiDevTable allcpu;
struct isiConTable allcon;
struct isiObjTable allobj;

static uint32_t maxsid = 0;

static int isi_insertindex_dev(struct isiInfo *dev, int32_t index, struct isiInfo *downdev, int32_t rindex);
static int isi_appendindex_dev(struct isiInfo *dev, struct isiInfo *downdev, int32_t *downindexout, int32_t rindex);
static int isi_setindex_dev(struct isiInfo *dev, uint32_t index, struct isiInfo *downdev, int32_t downindex);
int isi_message_ses(struct isiSessionRef *sr, uint32_t oid, uint16_t *msg, int len);

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

static void isi_update_busmem(struct isiInfo *bus, struct isiInfo *mem)
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

int isi_is_memory(struct isiInfo const *item)
{
	return (item->id.objtype > 0x2000 && item->id.objtype < 0x2f00);
}
int isi_is_cpu(struct isiInfo const *item)
{
	return (item->id.objtype >= ISIT_CPU && item->id.objtype < ISIT_ENDCPU);
}
int isi_is_bus(struct isiInfo const *item)
{
	return (item->id.objtype >= ISIT_BUSDEV && item->id.objtype < ISIT_ENDBUSDEV);
}

int isi_message_dev(struct isiInfo *src, int32_t srcindex, uint16_t *m, int l, struct timespec mtime)
{
	if(!src || srcindex < ISIAT_LIMIT) return -1;
	struct isiInfo *dest = NULL;
	int32_t lsi = srcindex;
	if(srcindex == ISIAT_UP) {
		dest = src->updev.t;
		lsi = src->updev.i;
	} else if(srcindex == ISIAT_SESSION) {
		return isi_message_ses(&src->sesref, src->id.id, m, l);
	} else if(srcindex >= 0) {
		isi_getindex_devi(src, srcindex, &dest, &lsi);
	} else if(srcindex == -1) {
		dest = src;
	} else return -1;
	if(!dest || !dest->c->MsgIn) return -1;
	return dest->c->MsgIn(dest, src, lsi, m, l, mtime);
}

int isi_deattach(struct isiInfo *item, int32_t itempoint)
{
	if(!item || itempoint < ISIAT_LIMIT || itempoint == ISIAT_APPEND) return ISIERR_INVALIDPARAM;
	if(item->id.objtype < 0x2f00) return ISIERR_INVALIDPARAM; /* can't process weird things */
	struct isiInfo *dev = NULL;
	if(itempoint == ISIAT_UP) {
		dev = item->updev.t;
	} else if(itempoint == ISIAT_INSERTSTART) {
		return ISIERR_NOTSUPPORTED;
	} else if(isi_getindex_dev(item, itempoint, &dev)) {
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
	if(dev->id.objtype < 0x2f00) return 0; /* weird thing, we're done */
	if(rai == ISIAT_UP) {
		dev->updev.t = NULL;
		dev->updev.i = -1;
	} else if(rai >= 0 && !isi_getindex_dev(dev, rai, NULL)) {
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
	if(item->id.objtype < 0x2f00) return ISIERR_INVALIDPARAM; /* can't process weird things */
	if(isi_is_memory(dev)) { /* handle attaching memory */
		if(item->c->QueryAttach) {
			if((e = item->c->QueryAttach(item, itempoint, dev, devpoint))) {
				return e;
			}
		}
		item->mem = dev;
		if(isi_is_cpu(item) && item->c->Attach) item->c->Attach(item, itempoint, dev, devpoint);
		if(isi_is_bus(item)) isi_update_busmem(item, item->mem);
		return 0;
	}
	int skipq = 0;
	if(dev->id.objtype < 0x2f00) return ISIERR_NOCOMPAT; /* can't attach weird things to other things */
	if(skipq) {
	} else if(item->c->QueryAttach) {
		if((e = item->c->QueryAttach(item, itempoint, dev, devpoint))) {
			return e;
		}
	} else if(!isi_is_bus(item)) return ISIERR_NOCOMPAT; /* can't attach random things to devices */
	if(skipq) {
	} else if(dev->c->QueryAttach) { /* test the other device too */
		if((e = dev->c->QueryAttach(dev, devpoint, item, itempoint))) {
			return e;
		}
	}
	if(itempoint >= 0) {
		if(!isi_getindex_dev(item, itempoint, 0)) {
			return ISIERR_ATTACHINUSE;
		}
	}
	if(devpoint >= 0) {
		if(!isi_getindex_dev(dev, devpoint, 0)) {
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
	if(item->c->Attach) item->c->Attach(item, itempoint, dev, devpoint);
	if(dev->c->Attached) dev->c->Attached(dev, devpoint, item, itempoint);
	return 0;
}

void isi_objtable_init()
{
	allobj.limit = 256;
	allobj.count = 0;
	allobj.table = (struct objtype**)isi_alloc(allobj.limit * sizeof(void*));
}

int isi_objtable_add(struct objtype *obj)
{
	if(!obj) return -1;
	void *n;
	if(allobj.count >= allobj.limit) {
		n = isi_realloc(allobj.table, (allobj.limit + allobj.limit) * sizeof(void*));
		if(!n) return -5;
		allobj.limit += allobj.limit;
		allobj.table = (struct objtype**)n;
	}
	allobj.table[allobj.count++] = obj;
	return 0;
}

int isi_find_obj(uint32_t id, struct objtype **target)
{
	size_t i;
	if(!id) return -1;
	for(i = 0; i < allobj.count; i++) {
		struct objtype *obj = allobj.table[i];
		if(obj && obj->id == id) {
			if(target) {
				*target = obj;
			}
			return 0;
		}
	}
	return 1;
}

int isi_find_uuid(uint32_t cid, uint64_t uuid, struct objtype **target)
{
	size_t i;
	if(!cid || !uuid) return -1;
	for(i = 0; i < allobj.count; i++) {
		struct objtype *obj = allobj.table[i];
		if(obj && obj->objtype == cid && obj->uuid == uuid) {
			if(target) {
				*target = obj;
			}
			return 0;
		}
	}
	return 1;
}

int isi_get_type_size(int objtype, size_t *sz)
{
	size_t objsize = 0;
	if( (objtype >> 12) > 2 ) objtype &= ~0xfff;
	switch(objtype) {
	case ISIT_NONE: return ISIERR_NOTFOUND;
	case ISIT_SESSION: objsize = sizeof(struct isiSession); break;
	case ISIT_NETSYNC: objsize = sizeof(struct isiNetSync); break;
	case ISIT_CEMEI: objsize = sizeof(struct isiInfo); break;
	case ISIT_DISK: objsize = sizeof(struct isiDisk); break;
	case ISIT_MEM6416: objsize = sizeof(struct memory64x16); break;
	case ISIT_CPU: objsize = sizeof(struct isiCPUInfo); break;
	case ISIT_BUSDEV: objsize = sizeof(struct isiInfo); break;
	case ISIT_HARDWARE: objsize = sizeof(struct isiInfo); break;
	}
	if(objsize) {
		*sz = objsize;
		return 0;
	}
	return ISIERR_NOTFOUND;
}

int isi_create_object(int objtype, struct objtype **out)
{
	if(!out) return -1;
	struct objtype *ns;
	size_t objsize = 0;
	if(isi_get_type_size(objtype, &objsize)) return ISIERR_INVALIDPARAM;
	ns = (struct objtype*)isi_alloc(objsize);
	if(!ns) return ISIERR_NOMEM;
	memset(ns, 0, objsize);
	ns->id = ++maxsid; // TODO make "better" ID numbers?
	ns->objtype = objtype;
	isi_objtable_add(ns);
	*out = ns;
	return 0;
}

static struct isiInfoCalls emptyobject = { 0, };
int isi_premake_object(int objtype, struct isiConstruct **outcon, struct objtype **out)
{
	uint32_t x;
	int i;
	int rs = 0;
	struct isiConstruct *con = NULL;
	if(!objtype || !out) return ISIERR_INVALIDPARAM;
	for(x = 0; x < allcon.count; x++) {
		if(allcon.table[x]->objtype == objtype) {
			con = allcon.table[x];
			break;
		}
	}
	if(!con) return ISIERR_NOTFOUND;
	*outcon = con;
	struct objtype *ndev;
	i = isi_create_object(objtype, &ndev);
	if(i) return i;
	struct isiInfo *info = (struct isiInfo*)ndev;
	if(objtype < 0x2f00) {
		*out = ndev;
		return 0;
	}
	info->meta = con;
	info->rvproto = con->rvproto;
	info->svproto = con->svproto;
	info->c = &emptyobject;
	if(con->PreInit) {
		rs = con->PreInit(info);
		if(rs) {
			isi_delete_object(ndev);
			*out = 0;
			return rs;
		}
	}
	if(objtype > 0x2f00 && con->QuerySize) {
		size_t sz = 0;
		con->QuerySize(0, &sz);
		if(sz) {
			info->rvstate = isi_alloc(sz);
		}
		sz = 0;
		con->QuerySize(1, &sz);
		if(sz) {
			info->svstate = isi_alloc(sz);
		}
	} else {
		if(con->rvproto && con->rvproto->length) {
			info->rvstate = isi_alloc(con->rvproto->length);
		}
		if(con->svproto && con->svproto->length) {
			info->svstate = isi_alloc(con->svproto->length);
		}
	}
	if(con->Init) {
		rs = con->Init(info);
		if(rs) {
			isi_delete_object(ndev);
			*out = 0;
			return rs;
		}
	}
	*out = ndev;
	return 0;
}

int isi_make_object(int objtype, struct objtype **out, const uint8_t *cfg, size_t lcfg)
{
	int rs = 0;
	struct objtype *ndev;
	struct isiConstruct *con = NULL;
	rs = isi_premake_object(objtype, &con, &ndev);
	if(rs) return rs;
	if(objtype < 0x2f00) {
		*out = ndev;
		return 0;
	}
	if(con->New) {
		rs = con->New((struct isiInfo *)ndev, cfg, lcfg);
		if(rs) {
			isi_delete_object(ndev);
			*out = 0;
			return rs;
		}
	}
	*out = ndev;
	return 0;
}

int isi_delete_object(struct objtype *obj)
{
	if(!obj) return ISIERR_INVALIDPARAM;
	struct isiObjTable *t = &allobj;
	if(obj->objtype >= 0x2fff) {
		struct isiInfo *info = (struct isiInfo *)obj;
		if(info->c->Delete) {
			info->c->Delete(info);
		}
		if(info->rvstate) {
			free(info->rvstate);
		}
		if(info->svstate) {
			free(info->svstate);
		}
		memset(info, 0, sizeof(struct isiInfo));
	}
	free(obj);
	uint32_t i;
	for(i = 0; i < t->count; i++) {
		if(t->table[i] == obj) break;
	}
	if(i < t->count) t->count--; else return -1;
	while(i < t->count) {
		t->table[i] = t->table[i+1];
		i++;
	}
	return 0;
}

int isi_init_contable()
{
	struct isiConTable *t = &allcon;
	t->limit = 32;
	t->count = 0;
	t->table = (struct isiConstruct**)isi_alloc(t->limit * sizeof(void*));
	return 0;
}

int isi_contable_add(struct isiConstruct *obj)
{
	if(!obj) return -1;
	void *n;
	if(allcon.count >= allcon.limit) {
		n = isi_realloc(allcon.table, (allcon.limit + allcon.limit) * sizeof(void*));
		if(!n) return -5;
		allcon.limit += allcon.limit;
		allcon.table = (struct isiConstruct**)n;
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
	itype = obj->objtype & 0xf000;
	inum = obj->objtype & 0x0fff;
	for(uint32_t i = 0; i < allcon.count; i++) {
		int ltype = allcon.table[i]->objtype & 0xf000;
		int lnum = allcon.table[i]->objtype & 0x0fff;
		if(ltype == itype) {
			if(lnum >= inum) {
				inum = lnum + 1;
			}
		}
	}
	obj->objtype = (itype & 0xf000) | (inum & 0x0fff);
	isi_contable_add(obj);
	isilog(L_INFO, "object: %s registered as %04x\n", obj->name, obj->objtype);
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

int isi_getindex_devi(struct isiInfo *dev, uint32_t index, struct isiInfo **downdev, int32_t *downidx)
{
	if(!dev) return -1;
	if(index >= dev->busdev.count || index >= dev->busdev.limit) return -1;
	if(!dev->busdev.table) return -1;
	struct isiInfo *o;
	int32_t ix;
	if(index == ISIAT_UP) o = dev->updev.t;
	else if(index == ISIAT_APPEND) {
		if(dev->busdev.count < 1) return -1;
		o = dev->busdev.table[dev->busdev.count - 1].t;
		ix = dev->busdev.table[dev->busdev.count - 1].i;
	} else if(index < 0) {
		return -1;
	} else {
		o = dev->busdev.table[index].t;
		ix = dev->busdev.table[index].i;
	}
	if(!o) return -1;
	if(downdev) *downdev = o;
	if(downidx) *downidx = ix;
	return 0;
}
int isi_getindex_dev(struct isiInfo *dev, uint32_t index, struct isiInfo **downdev)
{
	return isi_getindex_devi(dev, index, downdev, 0);
}

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

int isi_push_dev(struct isiDevTable *t, struct isiInfo *d)
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

int isi_find_dev(struct isiDevTable *t, uint32_t id, struct isiInfo **target)
{
	size_t i;
	if(!id) return -1;
	for(i = 0; i < t->count; i++) {
		struct isiInfo *obj = t->table[i];
		if(obj && obj->id.id == id) {
			if(target) {
				*target = obj;
			}
			return 0;
		}
	}
	return 1;
}

int isi_createdev(struct isiInfo **ndev)
{
	return isi_create_object(ISIT_HARDWARE, (struct objtype**)ndev);
}

int isi_createcpu(struct isiCPUInfo **ndev)
{
	return isi_create_object(ISIT_CPU, (struct objtype**)ndev);
}


