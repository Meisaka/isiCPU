
#include "cputypes.h"

struct isiDevTable alldev;
struct isiDevTable allcpu;
struct isiConTable allcon;
struct isiObjTable allobj;

static uint32_t maxsid = 0;

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

int isi_attach(struct isiInfo *item, struct isiInfo *dev)
{
	int e = 0;
	if(!item || !dev) return -1;
	if(item->id.objtype < 0x2f00) return -1;
	if(item->id.objtype >= ISIT_CPU && item->id.objtype < ISIT_ENDCPU) {
		if(dev->id.objtype > 0x2000 && dev->id.objtype < 0x2f00) {
			if(item->c->QueryAttach) {
				if((e = item->c->QueryAttach(item, dev))) {
					return e;
				}
			}
			item->mem = dev;
			if(item->c->Attach) e = item->c->Attach(item, dev);
			return e;
		}
	}
	if(dev->id.objtype < 0x2f00) return -1;
	if(item->c->QueryAttach) {
		if((e = item->c->QueryAttach(item, dev))) {
			return e;
		}
	}
	if(item->id.objtype >= ISIT_BUSDEV && item->id.objtype < ISIT_ENDBUSDEV) {
	} else {
		item->dndev = dev;
	}
	dev->updev = item;
	if(item->id.objtype >= ISIT_CPU && item->id.objtype < ISIT_ENDCPU) {
		dev->hostcpu = item;
		dev->mem = item->mem;
	} else {
		dev->hostcpu = item->hostcpu;
		dev->mem = item->mem;
	}
	if(item->c->Attach) item->c->Attach(item, dev);
	if(dev->c->Attached) dev->c->Attached(dev, item);
	if(dev->id.objtype >= ISIT_BUSDEV && dev->id.objtype < ISIT_ENDBUSDEV) {
		size_t k;
		size_t hs;
		struct isiBusInfo *bus = (struct isiBusInfo*)dev;
		hs = bus->busdev.count;
		for(k = 0; k < hs; k++) {
			if(bus->busdev.table[k]) {
				bus->busdev.table[k]->mem = dev->mem;
				bus->busdev.table[k]->hostcpu = dev->hostcpu;
			}
		}
	}
	return 0;
}

void isi_objtable_init()
{
	allobj.limit = 256;
	allobj.count = 0;
	allobj.table = (struct objtype**)malloc(allobj.limit * sizeof(void*));
}

int isi_objtable_add(struct objtype *obj)
{
	if(!obj) return -1;
	void *n;
	if(allobj.count >= allobj.limit) {
		n = realloc(allobj.table, (allobj.limit + allobj.limit) * sizeof(void*));
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
	case ISIT_DISK: objsize = sizeof(struct isiDisk); break;
	case ISIT_MEM6416: objsize = sizeof(struct memory64x16); break;
	case ISIT_CPU: objsize = sizeof(struct isiCPUInfo); break;
	case ISIT_BUSDEV: objsize = sizeof(struct isiBusInfo); break;
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
	ns = (struct objtype*)malloc(objsize);
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
			info->rvstate = malloc(sz);
			memset(info->rvstate, 0, sz);
		}
		sz = 0;
		con->QuerySize(1, &sz);
		if(sz) {
			info->svstate = malloc(sz);
			memset(info->svstate, 0, sz);
		}
	} else {
		if(con->rvproto && con->rvproto->length) {
			info->rvstate = malloc(con->rvproto->length);
			memset(info->rvstate, 0, con->rvproto->length);
		}
		if(con->svproto && con->svproto->length) {
			info->svstate = malloc(con->svproto->length);
			memset(info->svstate, 0, con->svproto->length);
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
	t->table = (struct isiConstruct**)malloc(t->limit * sizeof(void*));
	return 0;
}

int isi_contable_add(struct isiConstruct *obj)
{
	if(!obj) return -1;
	void *n;
	if(allcon.count >= allcon.limit) {
		n = realloc(allcon.table, (allcon.limit + allcon.limit) * sizeof(void*));
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

int isi_inittable(struct isiDevTable *t)
{
	t->limit = 32;
	t->count = 0;
	t->table = (struct isiInfo**)malloc(t->limit * sizeof(void*));
	return 0;
}

int isi_push_dev(struct isiDevTable *t, struct isiInfo *d)
{
	if(!d) return -1;
	void *n;
	if(!t->limit || !t->table) isi_inittable(t);
	if(t->count >= t->limit) {
		n = realloc(t->table, (t->limit + t->limit) * sizeof(void*));
		if(!n) return -5;
		t->limit += t->limit;
		t->table = (struct isiInfo**)n;
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


