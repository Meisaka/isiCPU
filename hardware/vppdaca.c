
#include "../dcpuhw.h"

/* state while this device exists/active (runtime volatile) */
struct vppDACA_rvstate {
	uint16_t iword;
	uint16_t buttons;
	uint16_t axis[4]; /* stored as 16 bit, then converted */
};
ISIREFLECT(struct vppDACA_rvstate,
	ISIR(vppDACA_rvstate, uint16_t, iword)
	ISIR(vppDACA_rvstate, uint16_t, buttons)
	ISIR(vppDACA_rvstate, uint16_t, axis)
)

static int vppDACA_Init(struct isiInfo *info);
static int vppDACA_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg);
struct isidcpudev vppDACA_Meta = {0x0001,0x31e1daca,0x621caf3a}; /* v1 / D/A Ctrl Adptr / VeXXaN */
struct isiConstruct vppDACA_Con = {
	.objtype = 0x5000,
	.name = "vppdaca",
	.desc = "D/A Control Adapter",
	.Init = vppDACA_Init,
	.New = vppDACA_New,
	.rvproto = &ISIREFNAME(struct vppDACA_rvstate),
	.svproto = NULL,
	.meta = &vppDACA_Meta
};
void vppDACA_Register()
{
	isi_register(&vppDACA_Con);
}

static int vppDACA_Reset(struct isiInfo *info)
{
	struct vppDACA_rvstate *dev = (struct vppDACA_rvstate*)info->rvstate;
	dev->iword = 0;
	return 0;
}

static int vppDACA_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, isi_time_t mtime)
{
	return 0;
}

static int vppDACA_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, isi_time_t crun)
{
	struct vppDACA_rvstate *dev = (struct vppDACA_rvstate*)info->rvstate;
	switch(msg[0]) {
	case 0:
		msg[0] = (dev->axis[0] & 0xFF00) | ((dev->axis[1] >> 8) & 0xff);
		msg[1] = (dev->axis[2] & 0xFF00) | ((dev->axis[3] >> 8) & 0xff);
		msg[2] = dev->buttons;
		break;
	case 1:
		msg[0] = (dev->axis[0] & 0xFF00) | ((dev->axis[1] >> 8) & 0xff);
		msg[1] = (dev->axis[2] & 0xFF00) | ((dev->axis[3] >> 8) & 0xff);
		break;
	case 2:
		msg[2] = dev->buttons;
		break;
	case 3:
		dev->iword = msg[1];
		break;
	}
	return 0;
}

static int vppDACA_MsgIn(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, isi_time_t mtime)
{
	struct vppDACA_rvstate *dev = (struct vppDACA_rvstate*)info->rvstate;
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case ISE_RESET: return 0; /* CPU finished reset */
	case ISE_QINT: return vppDACA_Query(info, src, msg+2, mtime); /* HWQ executed */
	case ISE_XINT: return vppDACA_HWI(info, src, msg+2, mtime); /* HWI executed */
	case ISE_AXIS16:
		for(int i = 0; i < 4; i++) {
			if(len > 1 + i) dev->axis[i] = msg[1 + i];
			if(dev->axis[i]) {
				if(dev->axis[i] < 0x100) dev->axis[i] = 0x100;
				if(dev->axis[i] > 0xFE00) dev->axis[i] = 0xFE00;
			}
		}
		break;
	case ISE_KEYDOWN:
		if(msg[1] < 16) {
			dev->buttons |= 1 << msg[1];
			if(dev->iword) { /* not in spec how to disable interrupts, assume 0 does it */
				uint16_t iom[3];
				iom[0] = ISE_XINT;
				iom[1] = 0;
				iom[2] = dev->iword;
				isi_message_dev(info, ISIAT_UP, iom, 3, mtime);
			}
		}
		break;
	case ISE_KEYUP:
		if(msg[1] < 16) {
			dev->buttons &= ~(1 << msg[1]);
		}
		break;
	default: break;
	}
	return 1;
}

static struct isiInfoCalls vppDACA_Calls = {
	.Reset = vppDACA_Reset, /* power on reset */
	.MsgIn = vppDACA_MsgIn, /* message from CPU or network */
};

static int vppDACA_Init(struct isiInfo *info)
{
	info->c = &vppDACA_Calls;
	return 0;
}

static int vppDACA_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg)
{
	return 0;
}

