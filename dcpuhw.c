
#include "dcpuhw.h"
#include <string.h>
#define DEBUG_DCPUHW 1

static int HWM_Init(struct isiInfo *info);
static struct isiConstruct DCPUBUS_Con = {
	.objtype = ISIT_BUSDEV,
	.name = "dcpu_hwbus",
	.desc = "DCPU Hardware backplane",
	.Init = HWM_Init,
};
void DCPUBUS_Register()
{
	isi_register(&DCPUBUS_Con);
}

static int HWM_FreeAll(struct isiInfo *info)
{
	if(!info) return -1;
	if(info->busdev.table) {
		isilog(L_DEBUG, "hwm: TODO correct free HW mem\n");
		free(info->busdev.table);
		info->busdev.table = NULL;
	}
	if(info->rvstate) {
		free(info->rvstate);
		info->rvstate = NULL;
	}
	return 0;
}

static int HWM_QAttach(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!dev) return -1;
	if(point == ISIAT_UP) return -1;
	if(dev->id.objtype == ISIT_MEM6416) return 0;
	if(info->busdev.count < 1 && dev->id.objtype != ISIT_DCPU) return ISIERR_MISSPREREQ;
	return 0;
}
static int HWM_DeviceAdd(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!dev) return -1;
	if(point == ISIAT_UP) return -1;
	isilog(L_DEBUG, "hwm: adding device c=%d n=%s\n", info->busdev.count, dev->meta->name);
	return 0;
}

static int HWM_Attached(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	isilog(L_DEBUG, "hwm: updating attachments c=%d\n", info->busdev.count);
	return 0;
}

static int HWM_Query(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	int r;
	r = 0;
	size_t h;
	size_t hs;
	h = msg[1];
	struct isiInfo *dev;
	hs = info->busdev.count;
	if(hs > 0 && src == info->busdev.table[0].t) {
		h++;
	} else {
		msg[1] = msg[0];
		msg++;
		len--;
	}

	// call it in context
	switch(msg[0]) {
	case ISE_RESET:
		msg[1] = (uint16_t)(hs - 1);
		isilog(L_DEBUG, "hwm-reset-all c=%ld\n", hs);
		for(h = 1; h < hs; h++) {
			if(!isi_getindex_dev(info, h, &dev)) {
				if(dev->c->Reset) dev->c->Reset(dev);
				if(!isi_message_dev(info, h, msg, len, mtime)) {
					isilog(L_DEBUG, "hwm-reset: %s %ld\n", dev->meta->name, h);
				}
			}
		}
		return 0;
	case ISE_QINT:
	case ISE_XINT:
		if(h >= hs) {
			isilog(L_DEBUG, "hwm: %ld out of range.\n", h);
			r = -1;
			break;
		}
		if(!isi_getindex_dev(info, h, &dev)) {
			if(msg[0] == ISE_QINT && dev->meta->meta) {
				struct isidcpudev *mid = (struct isidcpudev *)dev->meta->meta;
				msg[2] = (uint16_t)(mid->devid);
				msg[3] = (uint16_t)(mid->devid >> 16);
				msg[4] = mid->verid;
				msg[5] = (uint16_t)(mid->mfgid);
				msg[6] = (uint16_t)(mid->mfgid >> 16);
				r = 0;
			}
			r = isi_message_dev(info, h, msg, len, mtime);
		} else r = -1;
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
	hs = info->busdev.count;
	for(k = 1; k < hs; k++) {
		if(!isi_getindex_dev(info, k, &dev)) {
			if(dev->c->RunCycles) {
				dev->c->RunCycles(dev, crun);
			}
		}
	}
	return 0;
}

static struct isiInfoCalls HWMCalls = {
	.RunCycles = HWM_Run,
	.MsgIn = HWM_Query,
	.QueryAttach = HWM_QAttach,
	.Attach = HWM_DeviceAdd,
	.Attached = HWM_Attached,
	.Delete = HWM_FreeAll
};

static int HWM_Init(struct isiInfo *info)
{
	if(!info) return -1;
	isilog(L_DEBUG, "hwm: init bus\n");
	info->c = &HWMCalls;
	return 0;
}

