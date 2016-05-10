
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "cputypes.h"

static struct isiSyncTable {
	struct isiNetSync ** table;
	uint32_t count;
	uint32_t limit;
	uint8_t * out;
} allsync;

extern struct isiObjTable allobj;
extern struct isiSessionTable allses;

void isi_synctable_init()
{
	allsync.limit = 128;
	allsync.count = 0;
	allsync.table = (struct isiNetSync**)malloc(allsync.limit * sizeof(void*));
	allsync.out = (uint8_t *)malloc(2048);
}

int isi_synctable_add(struct isiNetSync *sync)
{
	if(!sync) return -1;
	void *n;
	if(allsync.count >= allsync.limit) {
		n = realloc(allsync.table, (allsync.limit + allsync.limit) * sizeof(void*));
		if(!n) return -5;
		allsync.limit += allsync.limit;
		allsync.table = (struct isiNetSync**)n;
	}
	allsync.table[allsync.count++] = sync;
	// TODO could probably sort this better, based on rate/id/hash.
	return 0;
}

int isi_create_sync(struct isiNetSync **sync)
{
	return isi_create_object(ISIT_NETSYNC, (struct objtype**)sync);
}

int isi_find_sync(struct objtype *target, struct isiNetSync **sync)
{
	size_t i;
	if(!target) return 0;
	for(i = 0; i < allsync.count; i++) {
		struct isiNetSync *ns = allsync.table[i];
		if(ns && (ns->target.id == target->id || ns->memobj.id == target->id)) {
			if(sync) {
				*sync = ns;
			}
			return 1;
		}
	}
	return 0;
}

int isi_find_devmem_sync(struct objtype *target, struct objtype *memtgt, struct isiNetSync **sync)
{
	size_t i;
	if(!target) return 0;
	for(i = 0; i < allsync.count; i++) {
		struct isiNetSync *ns = allsync.table[i];
		if(ns && ns->synctype == ISIN_SYNC_MEMDEV
			&& ns->target.id == target->id
			&& ns->memobj.id == memtgt->id) {
			if(sync) {
				*sync = ns;
			}
			return 1;
		}
	}
	return 0;
}

int isi_add_memsync(struct objtype *target, uint32_t base, uint32_t extent, size_t rate)
{
	struct isiNetSync *ns = 0;
	if(target->objtype < 0x2000 || target->objtype > 0x2fff) return -1;
	if(!isi_find_sync(target, &ns)) {
		isi_create_sync(&ns);
		ns->memobj.id = target->id;
		ns->memobj.objtype = target->objtype;
		isi_synctable_add(ns);
		ns->synctype = ISIN_SYNC_MEM;
	}
	ns->rate = rate;
	if(ns->extents >= 4) return -1;
	fprintf(stderr, "netsync: adding extent to sync [0x%04x +0x%04x]\n", base, extent);
	ns->base[ns->extents] = base;
	ns->len[ns->extents] = extent;
	ns->extents++;
	ns->ctl = 0;
	return ns->extents;
}

int isi_add_sync_extent(struct objtype *target, uint32_t base, uint32_t extent)
{
	struct isiNetSync *ns = 0;
	if(target->objtype < 0x2000 || target->objtype > 0x2fff) return -1;
	if(!isi_find_sync(target, &ns)) {
		isi_create_sync(&ns);
		ns->memobj.id = target->id;
		ns->memobj.objtype = target->objtype;
		isi_synctable_add(ns);
		ns->rate = 1000;
		ns->synctype = ISIN_SYNC_MEM;
	}
	if(ns->extents >= 4) return -1;
	fprintf(stderr, "netsync: adding extent to sync [0x%04x +0x%04x]\n", base, extent);
	ns->base[ns->extents] = base;
	ns->len[ns->extents] = extent;
	ns->extents++;
	ns->ctl = 0;
	return ns->extents - 1;
}

int isi_set_sync_extent(struct objtype *target, uint32_t index, uint32_t base, uint32_t extent)
{
	struct isiNetSync *ns = 0;
	if(target->objtype < 0x2000 || target->objtype > 0x2fff) return -1;
	if(!isi_find_sync(target, &ns)) {
		isi_create_sync(&ns);
		ns->memobj.id = target->id;
		ns->memobj.objtype = target->objtype;
		isi_synctable_add(ns);
		ns->rate = 10000;
		ns->synctype = ISIN_SYNC_MEM;
	}
	if(index > 3) return -1;
	if(index+1 > ns->extents) ns->extents = index+1;
	if(ns->extents > 4) return -1;
	fprintf(stderr, "netsync: adding extent to sync [%d][0x%04x +0x%04x]\n", index, base, extent);
	ns->base[index] = base;
	ns->len[index] = extent;
	ns->ctl = 0;
	return ns->extents - 1;
}

int isi_add_devsync(struct objtype *target, size_t rate)
{
	struct isiNetSync *ns = 0;
	if(!isi_find_sync(target, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
		isi_synctable_add(ns);
	}
	ns->synctype = ISIN_SYNC_DEVR;
	ns->rate = rate;
	ns->ctl = 0;
	return 0;
}

int isi_resync_dev(struct objtype *target)
{
	struct isiNetSync *ns = 0;
	if(!isi_find_sync(target, &ns)) {
		return -1;
	}
	ns->ctl &= ~8;
	return 0;
}

int isi_resync_all()
{
	size_t i;
	for(i = 0; i < allsync.count; i++) {
		struct isiNetSync *ns = allsync.table[i];
		if(ns && ns->ctl) {
			ns->ctl = 0;
		}
	}
	return 0;
}

int isi_add_devmemsync(struct objtype *target, struct objtype *memtarget, size_t rate)
{
	struct isiNetSync *ns = 0;
	if(memtarget->objtype < 0x2000 || memtarget->objtype > 0x2fff) return -1;
	if(!isi_find_devmem_sync(target, memtarget, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
		ns->memobj.id = memtarget->id;
		ns->memobj.objtype = memtarget->objtype;
		isi_synctable_add(ns);
		ns->synctype = ISIN_SYNC_MEMDEV;
	}
	ns->rate = rate;
	fprintf(stderr, "netsync: adding memdev sync rate=%ld \n", rate);
	ns->ctl = 0;
	return 0;
}

int isi_set_devmemsync_extent(struct objtype *target, struct objtype *memtarget, uint32_t index, uint32_t base, uint32_t extent)
{
	struct isiNetSync *ns = 0;
	if(memtarget->objtype < 0x2000 || memtarget->objtype > 0x2fff) return -1;
	if(!isi_find_devmem_sync(target, memtarget, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
		ns->memobj.id = memtarget->id;
		ns->memobj.objtype = memtarget->objtype;
		ns->synctype = ISIN_SYNC_MEMDEV;
		ns->rate = 50000000;
		isi_synctable_add(ns);
	}
	if(index > 3) return -1;
	if(index+1 > ns->extents) ns->extents = index+1;
	if(ns->extents > 4) return -1;
	ns->ctl = 0;
	fprintf(stderr, "netsync: adding extent to sync [%d][0x%04x +0x%04x]\n", index, base, extent);
	ns->base[index] = base;
	ns->len[index] = extent;
	return ns->extents - 1;
}

int isi_remove_sync(struct objtype *target)
{
	struct isiNetSync *ns = 0;
	if(!isi_find_sync(target, &ns)) {
		fprintf(stderr, "netsync: request to remove non-existent sync\n");
	}
	fprintf(stderr, "netsync: TODO remove sync\n");
	return 0;
}

int isi_find_obj_index(struct objtype *target)
{
	size_t i;
	if(!target) return -1;
	for(i = 0; i < allobj.count; i++) {
		struct objtype *obj = allobj.table[i];
		if(obj && obj->id == target->id) {
			return i;
		}
	}
	return -1;
}

void isi_run_sync(struct timespec crun)
{
	size_t i, k;
	int x;
	for(i = 0; i < allsync.count; i++) {
		struct isiNetSync *ns = allsync.table[i];
		struct memory64x16 *mem = 0;
		struct isiInfo *dev = 0;
		if(!ns) continue;
		if(!isi_time_lt(&ns->nrun, &crun))
			continue;
		if(ns->ctl) { /* check cache indexes */
			if((ns->ctl & 1) && (
				!ns->target.id
				|| !allobj.table[ns->target_index]
				|| allobj.table[ns->target_index]->id != ns->target.id))
			{
				ns->ctl ^= 1;
				fprintf(stderr, "netsync: invalidated index\n");
			}
			if((ns->ctl & 2) && (
				!ns->memobj.id
				|| !allobj.table[ns->memobj_index]
				|| allobj.table[ns->memobj_index]->id != ns->memobj.id))
			{
				ns->ctl &= ~(4|2);
				fprintf(stderr, "netsync: invalidated index\n");
			}
		}
		switch(ns->synctype) { /* update indexes */
		case ISIN_SYNC_MEM:
		case ISIN_SYNC_MEMDEV:
			if(!(ns->ctl & 2)) {
				x = isi_find_obj_index(&ns->memobj);
				if(x < 0) break;
				ns->memobj_index = x;
				ns->ctl |= 2;
				fprintf(stderr, "netsync: adding mem index %d\n", ns->ctl);
			}
			if((ns->ctl & 2)) mem = (isiram16)allobj.table[ns->memobj_index];
			if(mem && !(ns->ctl & 4)) {
				uint32_t mask = 0xffff;
				uint32_t addr;
				uint32_t alen;
				int ex, z;
				uint32_t idx;
				for(ex = ns->extents; ex--; ) {
					addr = ns->base[ex];
					alen = ns->len[ex];
					for(z = 0; z < alen; z++) {
						idx = (addr+z) & mask;
						mem->ctl[idx] &= 0xffff0000;
						mem->ctl[idx] |= (ISI_RAMCTL_DELTA|ISI_RAMCTL_SYNC) | ((~mem->ram[idx]) & 0xffff);
					}
				}
				mem->info |= ISI_RAMINFO_SCAN;
				ns->ctl |= 4;
			}
			if(ns->synctype != ISIN_SYNC_MEMDEV) break;
		case ISIN_SYNC_DEVR:
		case ISIN_SYNC_DEVV:
		case ISIN_SYNC_DEVRV:
			if(!(ns->ctl & 1)) {
				x = isi_find_obj_index(&ns->target);
				if(x < 0) break;
				ns->target_index = x;
				ns->ctl |= 1;
				fprintf(stderr, "netsync: adding dev index %d\n", ns->ctl);
			}
			if((ns->ctl & 1)) dev = (struct isiInfo *)allobj.table[ns->target_index];
			break;
		}
		switch(ns->synctype) {
		case ISIN_SYNC_MEM:
		case ISIN_SYNC_MEMDEV:
			if((ns->ctl & 4) && (mem->info & ISI_RAMINFO_SCAN) ) {
				uint32_t mask = 0xffff;
				uint32_t addr;
				uint32_t alen;
				uint32_t idx;
				int ex, z;
				uint32_t sta = 0;
				uint32_t sln = 0;
				uint16_t *mw = (uint16_t*)(allsync.out);
				for(ex = ns->extents; ex--; ) {
					addr = ns->base[ex];
					alen = ns->len[ex];
					sta = 0;
					sln = 0;
					for(z = 0; z < alen; z++) {
						idx = (addr+z) & mask;
						if(((mem->ram[idx] & 0xffff) ^ (mem->ctl[idx] & 0xffff))) {

							if(!sln) {
								sta = (addr+z);
								sln = 1;
							}
							if(sln) sln = 1+(addr+z)-sta; 
							mem->ctl[idx] &= 0xfffe0000;
							mem->ctl[idx] |= (mem->ram[idx] & 0xffffu);
						}
					}
					if(sln) {
						*(uint32_t*)(allsync.out+4) = mem->id.id;
						*(uint32_t*)(allsync.out+8) = sta << 1;
						for(z = 0; z < sln; z++) {
							mw[6+z] = mem->ram[(sta+z)&mask] & 0xffff;
						}
						*(uint32_t*)(allsync.out) = 0x0E100000|(8+(sln<<1));
						for(k = 0; k < allses.count; k++) {
							struct isiSession *ses;
							ses = allses.table[k];
							if(!ses) continue;
							session_write_msgex(ses, allsync.out);
						}
					}
				}
				mem->info ^= 1;
			}
			if((ns->ctl & 1) && !(ns->ctl & 8)) {
				ns->ctl |= 8;
				struct isiReflection *rfl;
				if(dev->rvproto) {
					rfl = dev->rvproto;
					*(uint32_t*)(allsync.out+4) = dev->id.id;
					memcpy(allsync.out+8, dev->rvstate, rfl->length);
					*(uint32_t*)(allsync.out) = 0x0E200000|(4+(rfl->length));
					for(k = 0; k < allses.count; k++) {
						struct isiSession *ses;
						ses = allses.table[k];
						if(!ses) continue;
						session_write_msgex(ses, allsync.out);
					}
				}
				if(dev->svproto) {
					rfl = dev->svproto;
					*(uint32_t*)(allsync.out+4) = dev->id.id;
					memcpy(allsync.out+8, dev->svstate, rfl->length);
					*(uint32_t*)(allsync.out) = 0x0E300000|(4+(rfl->length));
					for(k = 0; k < allses.count; k++) {
						struct isiSession *ses;
						ses = allses.table[k];
						if(!ses) continue;
						session_write_msgex(ses, allsync.out);
					}
				}
			}
			break;
		case ISIN_SYNC_DEVR:
		case ISIN_SYNC_DEVV:
		case ISIN_SYNC_DEVRV:
			break;
		}
		isi_addtime(&ns->nrun, ns->rate);
		k = 10;
		while(--k && isi_time_lt(&ns->nrun, &crun)) {
			isi_addtime(&ns->nrun, ns->rate);
		}
		if(!k) {
			ns->nrun.tv_sec = crun.tv_sec;
			ns->nrun.tv_nsec = crun.tv_nsec;
		}
	}
}

void isi_debug_dump_synctable()
{
	uint32_t i = 0;
	while(i < allsync.count) {
		fprintf(stderr, "sync-list: [%08x]: %x\n", allsync.table[i]->id.id, allsync.table[i]->id.objtype);
		fprintf(stderr, ": rate=%ld\n", allsync.table[i]->rate);
		i++;
	}
}

