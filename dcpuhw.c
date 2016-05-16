
#include "dcpuhw.h"
#include <string.h>
#define DEBUG_DCPUHW 1

static int HWM_Init(struct isiInfo *info, const uint8_t *cfg, size_t lcfg);
static struct isiConstruct DCPUBUS_Con = {
	ISIT_BUSDEV, "dcpu_hwbus", "DCPU Hardware backplane",
	0, HWM_Init, 0,
	NULL, NULL,
	0
};
void DCPUBUS_Register()
{
	isi_register(&DCPUBUS_Con);
}

static int HWM_FreeAll(struct isiInfo *info)
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

static int HWM_DeviceAdd(struct isiInfo *info, struct isiInfo *dev)
{
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	if(!dev) return -1;
	isi_push_dev(&bus->busdev, dev);
#if DEBUG_DCPUHW == 1
	fprintf(stderr, "hwm: adding device c=%d\n", bus->busdev.count);
#endif
	return 0;
}

static int HWM_Attached(struct isiInfo *info, struct isiInfo *dev)
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
			bus->busdev.table[k]->mem = info->mem;
			bus->busdev.table[k]->hostcpu = info->hostcpu;
		}
	}
	return 0;
}

static int HWM_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	int r;
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
			if(dev->Reset) dev->Reset(dev);
			if(dev->MsgIn) {
#if DEBUG_DCPUHW == 1
				fprintf(stderr, "hwm-reset: %s %ld\n", dev->meta->name, h);
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
		if(msg[0] == 1 && dev->meta->meta) {
			struct isidcpudev *mid = (struct isidcpudev *)dev->meta->meta;
			msg[2] = (uint16_t)(mid->devid);
			msg[3] = (uint16_t)(mid->devid >> 16);
			msg[4] = mid->verid;
			msg[5] = (uint16_t)(mid->mfgid);
			msg[6] = (uint16_t)(mid->mfgid >> 16);
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

static int HWM_Run(struct isiInfo *info, struct timespec crun)
{
	size_t k;
	size_t hs;
	struct isiInfo *dev;
	struct isiBusInfo *bus = (struct isiBusInfo*)info;
	hs = bus->busdev.count;
	for(k = 0; k < hs; k++) {
		dev = bus->busdev.table[k];
		if(dev->RunCycles) {
			dev->RunCycles(dev, crun);
		}
	}
	return 0;
}

static int HWM_Init(struct isiInfo *info, const uint8_t *cfg, size_t lcfg)
{
	if(!info) return -1;
#if DEBUG_MEM == 1
	fprintf(stderr, "hwm: init bus\n");
#endif
	info->RunCycles = HWM_Run;
	info->MsgIn = HWM_Query;
	info->Attach = HWM_DeviceAdd;
	info->Attached = HWM_Attached;
	info->Delete = HWM_FreeAll;
	return 0;
}

