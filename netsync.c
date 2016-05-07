
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "cputypes.h"

static struct isiSyncTable {
	struct isiNetSync ** table;
	uint32_t count;
	uint32_t limit;
} allsync;

void isi_synctable_init()
{
	allsync.limit = 128;
	allsync.count = 0;
	allsync.table = (struct isiNetSync**)malloc(allsync.limit * sizeof(void*));
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
		if(ns && ns->target.id == target->id) {
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
	if(!isi_find_sync(target, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
		isi_synctable_add(ns);
	}
	ns->synctype = ISIN_SYNC_MEM;
	ns->rate = rate;
	if(ns->extents >= 4) return -1;
	fprintf(stderr, "netsync: adding extent to sync [0x%04x +0x%04x]\n", base, extent);
	ns->base[ns->extents] = base;
	ns->len[ns->extents] = extent;
	ns->extents++;
	return ns->extents;
}

int isi_add_sync_extent(struct objtype *target, uint32_t base, uint32_t extent)
{
	struct isiNetSync *ns = 0;
	if(!isi_find_sync(target, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
		isi_synctable_add(ns);
		ns->rate = 1000;
		ns->synctype = ISIN_SYNC_MEM;
	}
	if(ns->extents >= 4) return -1;
	fprintf(stderr, "netsync: adding extent to sync [0x%04x +0x%04x]\n", base, extent);
	ns->base[ns->extents] = base;
	ns->len[ns->extents] = extent;
	ns->extents++;
	return ns->extents - 1;
}

int isi_set_sync_extent(struct objtype *target, uint32_t index, uint32_t base, uint32_t extent)
{
	struct isiNetSync *ns = 0;
	if(!isi_find_sync(target, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
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
	return 0;
}

int isi_add_devmemsync(struct objtype *target, struct objtype *memtarget, size_t rate)
{
	struct isiNetSync *ns = 0;
	if(!isi_find_devmem_sync(target, memtarget, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
		ns->memobj.id = memtarget->id;
		ns->memobj.objtype = memtarget->objtype;
		isi_synctable_add(ns);
	}
	ns->synctype = ISIN_SYNC_MEMDEV;
	ns->rate = rate;
	return 0;
}

int isi_set_devmemsync_extent(struct objtype *target, struct objtype *memtarget, uint32_t index, uint32_t base, uint32_t extent)
{
	struct isiNetSync *ns = 0;
	if(!isi_find_devmem_sync(target, memtarget, &ns)) {
		isi_create_sync(&ns);
		ns->target.id = target->id;
		ns->target.objtype = target->objtype;
		ns->memobj.id = memtarget->id;
		ns->memobj.objtype = memtarget->objtype;
		ns->synctype = ISIN_SYNC_MEMDEV;
		ns->rate = 10000;
		isi_synctable_add(ns);
	}
	if(index > 3) return -1;
	if(index+1 > ns->extents) ns->extents = index+1;
	if(ns->extents > 4) return -1;
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

void isi_debug_dump_synctable()
{
	uint32_t i = 0;
	while(i < allsync.count) {
		fprintf(stderr, "sync-list: [%08x]: %x\n", allsync.table[i]->id.id, allsync.table[i]->id.objtype);
		i++;
	}
}

