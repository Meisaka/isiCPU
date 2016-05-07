
#include "dcpuhw.h"
#include <string.h>
#define DEBUG_DCPUHW 1
extern struct stdevtable devtable[];

size_t HWM_GetSize(int idt, const char * cfg, size_t *sts, size_t *vs)
{
#if DEBUG_MEM == 1
	fprintf(stderr,"[%s] GetSize()\n", devtable[idt].devid_name);
#endif
	if((devtable[idt].flags & 0x6000) == 0x6000) {
		size_t s, v;
		s = devtable[idt].GetSize(0, cfg);
		v = devtable[idt].GetSize(1, cfg);
		if(sts) *sts = s;
		if(vs) *vs = v;
		return s;
	} else {
		return 0;
	}
}

int HWM_FreeAll(struct isiInfo *info)
{
	if(!info) return -1;
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	if(bus->busdev.table) {
		fprintf(stderr, "hwm: TODO correct free HW mem\n");
		free(bus->busdev.table);
		bus->busdev.table = NULL;
	}
	if(info->rvstate) {
		free(info->rvstate);
		info->rvstate = NULL;
	}
	return 0;
}

int HWM_DeviceAdd(struct isiInfo *info, struct isiInfo *dev)
{
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	dev->outdev = info->outdev;
	dev->mem = info->outdev->mem;
	isi_pushdev(&bus->busdev, dev);
#if DEBUG_DCPUHW == 1
	fprintf(stderr, "hwm: adding device c=%d\n", bus->busdev.count);
#endif
	return 0;
}

int HWM_Attached(struct isiInfo *info, struct isiInfo *dev)
{
	size_t k;
	size_t hs;
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
#if DEBUG_DCPUHW == 1
	fprintf(stderr, "hwm: updating attachment c=%d\n", bus->busdev.count);
#endif
	hs = bus->busdev.count;
	for(k = 0; k < hs; k++) {
		if(bus->busdev.table[k]) {
			bus->busdev.table[k]->outdev = dev;
			bus->busdev.table[k]->mem = dev->mem;
		}
	}
	return 0;
}

int HWM_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	int knt, r;
	r = 1;
	size_t h;
	size_t hs;
	h = msg[1];
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	struct isiInfo *dev;
	hs = bus->busdev.count;

	// call it in context
	switch(msg[0]) {
	case 0:
		msg[1] = (uint16_t)hs;
		fprintf(stderr, "hwm-reset-all c=%ld\n", hs);
		for(h = 0; h < hs; h++) {
			dev = bus->busdev.table[h];
			knt = dev->id.objtype - ISIT_DCPUHW;
			if(dev->Reset) dev->Reset(dev);
			if(dev->MsgIn) {
#if DEBUG_DCPUHW == 1
				fprintf(stderr, "hwm-reset: %s %ld\n", devtable[knt].devid_name, h);
#endif
				dev->MsgIn(dev, src, msg, mtime);
			}
		}
		return 0;
	case 1:
	case 2:
		if(h >= hs) {
#if DEBUG_DCPUHW == 1
			fprintf(stderr, "hwm: %ld out of range.\n", h);
#endif
			break;
		}
		dev = bus->busdev.table[h];
		knt = bus->busdev.table[h]->id.objtype - ISIT_DCPUHW;
		if(msg[0] == 1 && devtable[knt].flags & 0x8000) {
			msg[2] = (uint16_t)(devtable[knt].devid);
			msg[3] = (uint16_t)(devtable[knt].devid >> 16);
			msg[4] = devtable[knt].verid;
			msg[5] = (uint16_t)(devtable[knt].mfgid);
			msg[6] = (uint16_t)(devtable[knt].mfgid >> 16);
			r = 0;
		}
		if(dev->MsgIn) {
			r = dev->MsgIn(dev, src, msg, mtime);
		}
		return r;
	default:
		break;
	}
	return r;
}

int HWM_Run(struct isiInfo *info, struct timespec crun)
{
	size_t k;
	size_t hs;
	struct systemhwstate hws;
	struct isiInfo *dev;
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	hs = bus->busdev.count;
	for(k = 0; k < hs; k++) {
		dev = bus->busdev.table[k];
		if(dev->RunCycles) {
			if(dev->RunCycles(dev, crun)) {
				info->outdev->MsgIn(info->outdev, dev, &hws.msg, crun);
			}
		}
	}
	return 0;
}

int HWM_CreateDevice(struct isiInfo *info, const char *cfg)
{
	if(!info || !cfg) return -1;
	int i;
	i = 0;
	const char * cp;
	char *scn;
	const char * devname;
	cp = strchr(cfg, ':');
	devname = cfg;
	if(cp) {
		scn = strndup(cfg, cp - cfg);
		devname = scn;
	}
	while(devtable[i].flags) {
		if(strcmp(devname, devtable[i].devid_name) == 0) break;
		i++;
	}
	if(cp) {
		free(scn);
	}
	if(!devtable[i].flags) {
		fprintf(stderr, "hwm: device for \"%s\" not found\n", cfg);
		return -1;
	}
#if DEBUG_MEM == 1
	fprintf(stderr, "hwm: init hwdev\n");
#endif
	info->id.objtype = ISIT_DCPUHW + i;
	info->RunCycles = NULL;
	info->Reset = NULL;
	info->MsgIn = NULL;
	info->Attach = NULL;
	size_t sv, rv;
	HWM_GetSize(i, cfg, &rv, &sv);
	if(sv) {
		info->svstate = malloc(sv);
		memset(info->svstate, 0, sv);
	}
	if(rv) {
		info->rvstate = malloc(rv);
		memset(info->rvstate, 0, rv);
	}
	if(devtable[i].InitDev) {
		devtable[i].InitDev(info, cfg);
	}
	return 0;
}

int HWM_CreateBus(struct isiInfo *info)
{
	if(!info) return -1;
#if DEBUG_MEM == 1
	fprintf(stderr, "hwm: init bus\n");
#endif
	info->id.objtype = ISIT_DCPUBUS;
	info->RunCycles = HWM_Run;
	info->Reset = NULL;
	info->MsgIn = HWM_Query;
	info->Attach = HWM_DeviceAdd;
	info->Attached = HWM_Attached;
	return 0;
}

