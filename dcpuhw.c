
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
	isi_pushdev(&bus->busdev, dev);
#if DEBUG_DCPUHW == 1
	fprintf(stderr, "hwm: adding device c=%d\n", bus->busdev.count);
#endif
	return 0;
}

int HWM_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	int knt, r;
	r = HWQ_FAIL;
	size_t h;
	size_t hs;
	h = msg[1];
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	hs = bus->busdev.count;

	// call it in context
	switch(msg[0]) {
	case 0:
		msg[1] = (uint16_t)hs;
		struct isiInfo *dev;
		fprintf(stderr, "hwmreset-all c=%ld\n", hs);
		for(h = 0; h < hs; h++) {
			dev = bus->busdev.table[h];
			knt = dev->id.objtype - ISIT_DCPUHW;
			if((devtable[knt].flags & 0x4000) && devtable[knt].OnReset) {
#if DEBUG_DCPUHW == 1
		fprintf(stderr, "hwmreset: %s %ld\n", devtable[knt].devid_name, h);
#endif
				devtable[knt].OnReset(dev, src, mtime);
			}
		}
		return 1;
	case 1:
		if(h >= hs) {
#if DEBUG_DCPUHW == 1
			fprintf(stderr, "hwmhwq: %ld out of range.\n", h);
#endif
			break;
		}
		knt = bus->busdev.table[h]->id.objtype - ISIT_DCPUHW;
#if DEBUG_DCPUHW == 1
		fprintf(stderr, "hwmhwq: %s %ld %04x : ", devtable[knt].devid_name, h, devtable[knt].flags);
#endif
		if(devtable[knt].flags & 0x8000) {
			msg[2] = (uint16_t)(devtable[knt].devid);
			msg[3] = (uint16_t)(devtable[knt].devid >> 16);
			msg[4] = devtable[knt].verid;
			msg[5] = (uint16_t)(devtable[knt].mfgid);
			msg[6] = (uint16_t)(devtable[knt].mfgid >> 16);
			r = HWQ_SUCCESS;
		}
		if(devtable[knt].OnHWQ) {
			r = devtable[knt].OnHWQ(bus->busdev.table[h], src, msg+2, mtime);
#if DEBUG_DCPUHW == 1
		fprintf(stderr, "call: %d", r);
#endif
		}
#if DEBUG_DCPUHW == 1
		fprintf(stderr, ": %04x %04x %04x %04x %04x \n", msg[2], msg[3], msg[4], msg[5], msg[6]);
#endif
		return r;
	case 2:
		if(h >= hs) {
#if DEBUG_DCPUHW == 1
			fprintf(stderr, "hwmhwq: %ld out of range.\n", h);
#endif
			break;
		}
		knt = bus->busdev.table[h]->id.objtype - ISIT_DCPUHW;
		if((devtable[knt].flags & 0x4002) == 0x4002) {
#if DEBUG_DCPUHW == 1
			if(!(devtable[knt].flags & 0x0100))
				fprintf(stderr, "[HWIA %s %04x]\n", devtable[knt].devid_name, devtable[knt].flags);
#endif
			r = devtable[knt].OnHWI(bus->busdev.table[h], src, msg+2, mtime);
		} else {
#if DEBUG_DCPUHW == 1
			fprintf(stderr, "[HWIN %s %04x]\n", devtable[knt].devid_name, devtable[knt].flags);
#endif
		       r = 0;
		}
		return r;
	}
	return r;
}

int HWM_Run(struct isiInfo *info, struct isiSession *ses, struct timespec crun)
{
	size_t i, k;
	size_t hs;
	struct systemhwstate hws;
	hws.net = ses;
	hws.mem = (isiram16)((struct isiCPUInfo *)info->outdev)->mem;
	struct isiInfo *dev;
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	hs = bus->busdev.count;
	for(k = 0; k < hs; k++) {
		dev = bus->busdev.table[k];
		i = dev->id.objtype - ISIT_DCPUHW;
		if((devtable[i].flags & 0x4004) == 0x4004) {
			if(devtable[i].OnTick(dev, &hws, crun)) {
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
	return 0;
}

