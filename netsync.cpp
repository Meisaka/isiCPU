
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "isitypes.h"
#include "netmsg.h"

constexpr auto NSY_OBJ = 1;
constexpr auto NSY_MEM = 2;
constexpr auto NSY_MEMVALID = 4;
constexpr auto NSY_SYNCOBJ = 8;
constexpr auto NSY_SYNCMEM = 16;

static std::vector<isiNetSync *> allsync;

extern std::vector<isiObjSlot> allobj;
extern std::vector<isiSession*> allses;

int isi_synctable_add(isiNetSync *sync) {
	if(!sync) return -1;
	// TODO could probably sort this better, based on rate/id/hash.
	allsync.push_back(sync);
	return 0;
}

static isiClass<isiNetSync> isiNetSync_C(ISIT_NETSYNC, "<isiNetSync>", "");

void isi_register_netsync() {
	isi_register(&isiNetSync_C);
}

int isi_create_sync(isiNetSync **sync) {
	return isi_create_object(ISIT_NETSYNC, NULL, (isiObject**)sync);
}

int isi_find_sync(isiObject *target, isiNetSync **sync) {
	if(!target) return 0;
	for(auto ns : allsync) {
		if(ns && (ns->target.id == target->id || ns->memobj.id == target->id)) {
			if(sync) {
				*sync = ns;
			}
			return 1;
		}
	}
	return 0;
}

int isi_find_devmem_sync(isiObject *target, isiObject *memtgt, isiNetSync **sync) {
	if(!target) return 0;
	for(auto ns : allsync) {
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

int isi_add_memsync(isiObject *target, uint32_t base, uint32_t extent, size_t rate) {
	isiNetSync *ns = 0;
	int r;
	if(!isi_is_infodev(target)) return -1;
	if(!isi_find_sync(target, &ns)) {
		if((r = isi_create_sync(&ns))) return r;
		ns->memobj.id = target->id;
		ns->memobj.otype = target->otype;
		if((r = isi_synctable_add(ns))) return r;
		ns->synctype = ISIN_SYNC_MEM;
	}
	ns->rate = rate;
	if(ns->extents >= 4) return -1;
	isilog(L_DEBUG, "netsync: adding extent to sync [0x%04x +0x%04x]\n", base, extent);
	ns->base[ns->extents] = base;
	ns->len[ns->extents] = extent;
	ns->extents++;
	ns->ctl = 0;
	return ns->extents;
}

int isi_add_sync_extent(isiObject *target, uint32_t base, uint32_t extent) {
	isiNetSync *ns = 0;
	int r;
	if(!isi_is_infodev(target)) return -1;
	if(!isi_find_sync(target, &ns)) {
		if((r = isi_create_sync(&ns))) return r;
		ns->memobj.id = target->id;
		ns->memobj.otype = target->otype;
		if((r = isi_synctable_add(ns))) return r;
		ns->rate = 1000;
		ns->synctype = ISIN_SYNC_MEM;
	}
	if(ns->extents >= 4) return -1;
	isilog(L_DEBUG, "netsync: adding extent to sync [0x%04x +0x%04x]\n", base, extent);
	ns->base[ns->extents] = base;
	ns->len[ns->extents] = extent;
	ns->extents++;
	ns->ctl = 0;
	return ns->extents - 1;
}

int isi_set_sync_extent(isiObject *target, uint32_t index, uint32_t base, uint32_t extent) {
	isiNetSync *ns = 0;
	int r;
	if(!isi_is_infodev(target)) return -1;
	if(!isi_find_sync(target, &ns)) {
		if((r = isi_create_sync(&ns))) return r;
		ns->memobj.id = target->id;
		ns->memobj.otype = target->otype;
		if((r = isi_synctable_add(ns))) return r;
		ns->rate = 10000;
		ns->synctype = ISIN_SYNC_MEM;
	}
	if(index > 3) return -1;
	if(index+1 > ns->extents) ns->extents = index+1;
	if(ns->extents > 4) return -1;
	isilog(L_DEBUG, "netsync: adding extent to sync [%d][0x%04x +0x%04x]\n", index, base, extent);
	ns->base[index] = base;
	ns->len[index] = extent;
	ns->ctl = 0;
	return ns->extents - 1;
}

int isi_add_devsync(isiObject *target, size_t rate) {
	isiNetSync *ns = 0;
	int r;
	if(!isi_find_sync(target, &ns)) {
		if((r = isi_create_sync(&ns))) return r;
		ns->target.id = target->id;
		ns->target.otype = target->otype;
		if((r = isi_synctable_add(ns))) return r;
		ns->synctype = ISIN_SYNC_DEVR;
		ns->rate = rate;
		ns->ctl = 0;
		isilog(L_DEBUG, "netsync: adding dev sync, rate=%ld \n", rate);
	} else if(ns->rate != rate) {
		ns->rate = rate;
		isilog(L_DEBUG, "netsync: resetting memdev sync rate=%ld \n", rate);
	}
	ns->ctl &= ~NSY_SYNCOBJ;
	return 0;
}

int isi_resync_dev(isiObject *target) {
	isiNetSync *ns = 0;
	if(!isi_find_sync(target, &ns)) {
		return -1;
	}
	ns->ctl &= ~NSY_SYNCOBJ;
	return 0;
}

int isi_resync_all() {
	for(auto ns : allsync) {
		if(ns && ns->ctl) {
			ns->ctl = 0;
		}
	}
	return 0;
}

int isi_add_devmemsync(isiObject *target, isiObject *memtarget, size_t rate) {
	isiNetSync *ns = 0;
	int r;
	if(!isi_is_memory(memtarget)) return -1;
	if(!isi_find_devmem_sync(target, memtarget, &ns)) {
		if((r = isi_create_sync(&ns))) return r;
		ns->target.id = target->id;
		ns->target.otype = target->otype;
		ns->memobj.id = memtarget->id;
		ns->memobj.otype = memtarget->otype;
		if((r = isi_synctable_add(ns))) return r;
		ns->synctype = ISIN_SYNC_MEMDEV;
		ns->rate = rate;
		isilog(L_DEBUG, "netsync: adding memdev sync, rate=%ld \n", rate);
		ns->ctl = 0;
	} else if(ns->rate != rate) {
		ns->rate = rate;
		isilog(L_DEBUG, "netsync: resetting memdev sync rate=%ld \n", rate);
	}
	ns->ctl |= NSY_SYNCMEM;
	ns->ctl &= ~NSY_SYNCOBJ;
	return 0;
}

int isi_set_devmemsync_extent(isiObject *target, isiObject *memtarget, uint32_t index, uint32_t base, uint32_t extent) {
	isiNetSync *ns = 0;
	int r;
	if(!isi_is_memory(memtarget)) return -1;
	if(!isi_find_devmem_sync(target, memtarget, &ns)) {
		if((r = isi_create_sync(&ns))) return r;
		ns->target.id = target->id;
		ns->target.otype = target->otype;
		ns->memobj.id = memtarget->id;
		ns->memobj.otype = memtarget->otype;
		ns->synctype = ISIN_SYNC_MEMDEV;
		ns->rate = 50000000;
		if((r = isi_synctable_add(ns))) return r;
	}
	if(index > 3) return -1;
	if(index+1 > ns->extents) ns->extents = index+1;
	if(ns->extents > 4) return -1;
	ns->ctl |= NSY_SYNCMEM;
	ns->ctl &= ~NSY_SYNCOBJ;
	if(ns->base[index] != base || ns->len[index] != extent) {
		ns->ctl &= ~(NSY_MEM | NSY_MEMVALID);
		isilog(L_DEBUG, "netsync: adding extent to sync [%d][0x%04x +0x%04x]\n", index, base, extent);
		ns->base[index] = base;
		ns->len[index] = extent;
	}
	return ns->extents - 1;
}

int isi_remove_sync(isiObject *target) {
	isiNetSync *ns = 0;
	if(!isi_find_sync(target, &ns)) {
		isilog(L_DEBUG, "netsync: request to remove non-existent sync\n");
	}
	isilog(L_DEBUG, "netsync: TODO remove sync\n");
	return 0;
}

int isi_find_obj_index(isiObject *target) {
	size_t i;
	if(!target) return -1;
	for(i = 0; i < allobj.size(); i++) {
		isiObject *obj = allobj[i].ptr;
		if(obj && obj->id == target->id) {
			return i;
		}
	}
	return -1;
}

void isi_run_sync(isi_time_t crun) {
	size_t k;
	int x;
	isiMsgRef allsync_out = make_msg();
	for(isiNetSync *ns : allsync) {
		isiMemory *mem = nullptr;
		if(!ns) continue;
		if(ns->ctl) { /* check cache indexes */
			if((ns->ctl & NSY_OBJ) && (
				!ns->target.id
				|| !allobj[ns->target_index].ptr
				|| allobj[ns->target_index].ptr->id != ns->target.id))
			{
				ns->ctl ^= NSY_OBJ;
				isilog(L_DEBUG, "netsync: invalidated index\n");
			}
			if((ns->ctl & NSY_MEM) && (
				!ns->memobj.id
				|| !allobj[ns->memobj_index].ptr
				|| allobj[ns->memobj_index].ptr->id != ns->memobj.id))
			{
				ns->ctl &= ~(NSY_MEMVALID|NSY_MEM|NSY_SYNCMEM);
				isilog(L_DEBUG, "netsync: invalidated index\n");
			}
		}
		if((ns->ctl & NSY_MEM)) {
			mem = (isiMemory*)allobj[ns->memobj_index].ptr;
			if(mem->info & ISI_RAMINFO_SCAN) ns->ctl |= NSY_SYNCMEM;
		}
	}
	for(isiNetSync *ns : allsync) {
		isiMemory *mem = nullptr;
		isiInfo *dev = 0;
		if(!ns) continue;
		if(!isi_time_lt(&ns->nrun, &crun))
			continue;
		switch(ns->synctype) { /* update indexes */
		case ISIN_SYNC_MEM:
		case ISIN_SYNC_MEMDEV:
			if(!(ns->ctl & NSY_MEM)) {
				x = isi_find_obj_index(&ns->memobj);
				if(x < 0) break;
				ns->memobj_index = x;
				ns->ctl |= NSY_MEM;
				isilog(L_DEBUG, "netsync: adding mem index %x\n", ns->ctl);
			}
			if((ns->ctl & NSY_MEM)) {
				mem = (isiMemory*)allobj[ns->memobj_index].ptr;
				if(mem->info & ISI_RAMINFO_SCAN) mem->info ^= ISI_RAMINFO_SCAN;
			}
			if(mem && !(ns->ctl & NSY_MEMVALID)) {
				uint32_t addr;
				uint32_t alen;
				uint32_t ex, z;
				for(ex = ns->extents; ex--; ) {
					addr = ns->base[ex];
					alen = ns->len[ex];
					for(z = 0; z < alen; z++, addr++) {
						mem->sync_set(addr, ISI_RAMCTL_DELTA|ISI_RAMCTL_SYNC);
					}
				}
				mem->info |= ISI_RAMINFO_SCAN;
				ns->ctl |= NSY_MEMVALID;
			}
			if(ns->synctype != ISIN_SYNC_MEMDEV) break;
		case ISIN_SYNC_DEVR:
		case ISIN_SYNC_DEVV:
		case ISIN_SYNC_DEVRV:
			if(!(ns->ctl & NSY_OBJ)) {
				x = isi_find_obj_index(&ns->target);
				if(x < 0) break;
				ns->target_index = x;
				ns->ctl |= NSY_OBJ;
				isilog(L_DEBUG, "netsync: adding dev index %x\n", ns->ctl);
			}
			if((ns->ctl & NSY_OBJ)) dev = (isiInfo *)allobj[ns->target_index].ptr;
			break;
		}
		switch(ns->synctype) {
		case ISIN_SYNC_MEM:
		case ISIN_SYNC_MEMDEV:
			if((ns->ctl & NSY_MEMVALID) && (ns->ctl & NSY_SYNCMEM)) {
				uint32_t idx;
				uint32_t ex, z;
				bool is_full_sync = true;
				for(ex = ns->extents; ex--; ) {
					uint32_t addr = ns->base[ex];
					uint32_t alen = ns->len[ex];
					uint32_t sync_addr = 0;
					uint32_t sync_len = 0; // this is also if we are writing a block or not
					uint32_t nodelta_len = 0;
					allsync_out->u32[0] = mem->id;
					constexpr uint32_t SYNC32_RECLEN = 5; // mess with alignment >:)
					// TODO mem_t could be different, move this to a memory function?
					// move the data pointer to after the ID, where the blocks start
					uint16_t *data_start = (uint16_t*)(allsync_out->u8 + 4);
					uint16_t *data16 = data_start;
					uint8_t *sync_hdr;
					// reserve an extra 12 bytes at the end so we don't go over (yes, I'm lazy)
					uint16_t *wlimit = (uint16_t*)(allsync_out->u8+allsync_out->limit - 12);
					for(z = 0; z < alen; z++) {
						idx = addr;
						if(sync_len) { // add words of data while a sync block is active
							sync_len += 2;
							*data16 = mem->i_rd(addr);
							data16++;
						}
						// does this word of memory need synching? (clears delta flag)
						if(mem->sync_rd(addr) & ISI_RAMCTL_DELTA) {
							nodelta_len = 0;
							if(!sync_len) {
								// start a sync block to put this word in
								sync_addr = mem->byte_offset(addr);
								// make room for the header (it's written to later)
								data16 = (uint16_t*)(sync_hdr+SYNC32_RECLEN);
								// put the first word
								*data16 = mem->i_rd(addr);
								data16++;
								sync_len = 2;
							}
						} else {
							// keep track of how many words *don't* need updating
							if(sync_len) nodelta_len += 2;
						}
						// if we hit our limits or are synching more than we need to
						// the nodelta_len limit is a block with extra bytes vs size of 2 blocks
						if(data16 >= wlimit || nodelta_len >= 8 || sync_len >= 250) {
							// backup over words that don't need to be sent
							// nodelta_len is bytes so some casts are required
							data16 = (uint16_t*)(((uint8_t*)data16) - nodelta_len);
							sync_len -= nodelta_len;
							// write the header and close the block
							sync_hdr[0] = (uint8_t)(sync_addr);
							sync_hdr[1] = (uint8_t)(sync_addr>>8);
							sync_hdr[2] = (uint8_t)(sync_addr>>16);
							sync_hdr[3] = (uint8_t)(sync_addr>>24);
							sync_hdr[4] = (uint8_t)(sync_len);
							nodelta_len = sync_len = sync_addr = 0;
							if(data16 >= wlimit) {
								// need another pass to get everything
								is_full_sync = false;
								break;
							}
						}
					}
					if(sync_len) { // close the block if it's open
						data16 = (uint16_t*)(((uint8_t*)data16) - nodelta_len);
						sync_len -= nodelta_len;
						sync_hdr[0] = (uint8_t)(sync_addr);
						sync_hdr[1] = (uint8_t)(sync_addr>>8);
						sync_hdr[2] = (uint8_t)(sync_addr>>16);
						sync_hdr[3] = (uint8_t)(sync_addr>>24);
						sync_hdr[4] = (uint8_t)(sync_len);
						isilog(L_DEBUG, "netsync-m: %04x+%x\n", sync_addr, sync_len);
					}
					if(data16 > data_start) {
						allsync_out->code = ISIMSG(SYNCMEM32, 0);
						allsync_out->length = ((uint8_t*)data16) - allsync_out->u8;
						session_multicast_msg(allsync_out);
						allsync_out = make_msg();
					}
				}
				if(is_full_sync) {
					ns->ctl &= ~NSY_SYNCMEM;
				}
			}
		case ISIN_SYNC_DEVR:
		case ISIN_SYNC_DEVV:
		case ISIN_SYNC_DEVRV:
			if((ns->ctl & NSY_OBJ) && !(ns->ctl & NSY_SYNCOBJ)) {
				ns->ctl |= NSY_SYNCOBJ;
				isiReflection const *rfl;
				if(dev->meta->rvproto) {
					rfl = dev->meta->rvproto;
					allsync_out->u32[0] = dev->id;
					if(rfl->length > allsync_out->limit) {
						isilog(L_ERR, "sync: over write rv\n");
					}
					memcpy(allsync_out->u32 + 1, dev->rvstate, rfl->length);
					allsync_out->code = ISIMSG(SYNCRVS, 0);
					allsync_out->length = 4 + (rfl->length);
					session_multicast_msg(allsync_out);
					allsync_out = make_msg();
				}
				if(dev->meta->svproto) {
					rfl = dev->meta->svproto;
					allsync_out->u32[0] = dev->id;
					if(rfl->length > allsync_out->limit) {
						isilog(L_ERR, "sync: over write sv\n");
					}
					memcpy(allsync_out->u32 + 1, dev->svstate, rfl->length);
					allsync_out->code = ISIMSG(SYNCSVS, 0);
					allsync_out->length = 4 + (rfl->length);
					session_multicast_msg(allsync_out);
					allsync_out = make_msg();
				}
			}
			break;
		}
		isi_add_time(&ns->nrun, ns->rate);
		k = 10;
		while(--k && isi_time_lt(&ns->nrun, &crun)) {
			isi_add_time(&ns->nrun, ns->rate);
		}
		if(!k) {
			ns->nrun = crun;
		}
	}
}

void isi_debug_dump_synctable() {
	for(auto ns : allsync) {
		isilog(L_DEBUG, "sync-list: [%08x]: %x -> %#x\n", ns->id, ns->otype, ns->target_index);
		isilog(L_DEBUG, ": rate=%ld\n", ns->rate);
		isilog(L_DEBUG, ": block[0]=%08x +%x\n", ns->base[0], ns->len[0]);
	}
}

