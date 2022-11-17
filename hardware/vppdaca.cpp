
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

class vppDACA : public isiInfo {
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int Reset();
	int HWQ(isiInfo *src, uint32_t *msg, isi_time_t crun);
	int HWI(isiInfo *src, uint32_t *msg, isi_time_t crun);
};
struct isidcpudev vppDACA_Meta = {0x0001,0x31e1daca,0x621caf3a}; /* v1 / D/A Ctrl Adptr / VeXXaN */
isiClass<vppDACA> vppDACA_Con(
	ISIT_HARDWARE, "trk_vpp_daca", "D/A Control Adapter",
	&ISIREFNAME(struct vppDACA_rvstate),
	NULL,
	NULL,
	&vppDACA_Meta);
void vppDACA_Register()
{
	isi_register(&vppDACA_Con);
}

int vppDACA::Reset()
{
	struct vppDACA_rvstate *dev = (struct vppDACA_rvstate*)this->rvstate;
	dev->iword = 0;
	return 0;
}

int vppDACA::HWQ(isiInfo *src, uint32_t *msg, isi_time_t crun)
{
	return 0;
}
int vppDACA::HWI(isiInfo *src, uint32_t *msg, isi_time_t crun)
{
	struct vppDACA_rvstate *dev = (struct vppDACA_rvstate*)this->rvstate;
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

int vppDACA::MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	struct vppDACA_rvstate *dev = (struct vppDACA_rvstate*)this->rvstate;
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case ISE_RESET: return 0; /* CPU finished reset */
	case ISE_QINT: return this->HWQ(src, msg+2, mtime); /* HWQ executed */
	case ISE_XINT: return this->HWI(src, msg+2, mtime); /* HWI executed */
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
				uint32_t iom[3];
				iom[0] = ISE_XINT;
				iom[1] = 0;
				iom[2] = dev->iword;
				isi_message_dev(this, ISIAT_UP, iom, 3, mtime);
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

