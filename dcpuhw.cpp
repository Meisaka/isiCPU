
#include "dcpuhw.h"
#include <string.h>
#define DEBUG_DCPUHW 1

class DCPUBUS : public isiInfo {
public:
	DCPUBUS() {}
	virtual int Run(isi_time_t crun);
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int QueryAttach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Attach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	//virtual int Deattach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	//virtual int Reset(isiInfo *);
	virtual int Delete();
	virtual int on_attached(int32_t to_point, isiInfo *dev, int32_t from_point);
};

static isiClass<DCPUBUS> DCPUBUS_Con(ISIT_BUSDEV, "dcpu_hwbus", "DCPU Hardware backplane");

void DCPUBUS_Register() {
	isi_register(&DCPUBUS_Con);
}

int DCPUBUS::Delete() {
	if(this->busdev.table) {
		isilog(L_DEBUG, "hwm: TODO correct free HW mem\n");
		free(this->busdev.table);
		this->busdev.table = NULL;
	}
	if(this->rvstate) {
		free(this->rvstate);
		this->rvstate = NULL;
	}
	return 0;
}

int DCPUBUS::QueryAttach(int32_t point, isiInfo *dev, int32_t devpoint) {
	if(!dev) return -1;
	if(point == ISIAT_UP) return -1;
	if(dev->otype == ISIT_MEM16) return 0;
	if(this->busdev.count < 1 && !ISIT_IS(dev->otype, ISIT_CPU)) return ISIERR_MISSPREREQ;
	return 0;
}
int DCPUBUS::Attach(int32_t point, isiInfo *dev, int32_t devpoint) {
	if(!dev) return -1;
	if(point == ISIAT_UP) return -1;
	isilog(L_DEBUG, "hwm: adding device c=%d n=%s\n", this->busdev.count, dev->meta->name.data());
	return 0;
}

int DCPUBUS::on_attached(int32_t to_point, isiInfo *dev, int32_t from_point)
{
	isilog(L_DEBUG, "hwm: updating attachments c=%d\n", this->busdev.count);
	return 0;
}

int DCPUBUS::MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	int r;
	r = 0;
	size_t h;
	size_t hs;
	h = msg[1];
	isiInfo *dev;
	hs = this->busdev.count;
	if(hs > 0 && src == this->busdev.table[0].t) {
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
			if(!this->get_dev(h, &dev)) {
				dev->Reset();
				if(!isi_message_dev(this, h, msg, len, mtime)) {
					isilog(L_DEBUG, "hwm-reset: %s %ld\n", dev->meta->name.data(), h);
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
		if(!this->get_dev(h, &dev)) {
			if(msg[0] == ISE_QINT && dev->meta->meta) {
				struct isidcpudev *mid = (struct isidcpudev *)dev->meta->meta;
				msg[2] = (uint16_t)(mid->devid);
				msg[3] = (uint16_t)(mid->devid >> 16);
				msg[4] = mid->verid;
				msg[5] = (uint16_t)(mid->mfgid);
				msg[6] = (uint16_t)(mid->mfgid >> 16);
				r = 0;
			}
			r = isi_message_dev(this, h, msg, len, mtime);
		} else r = -1;
		return r;
	default:
		break;
	}
	return r;
}

int DCPUBUS::Run(isi_time_t crun)
{
	size_t k;
	size_t hs;
	isiInfo *dev;
	hs = this->busdev.count;
	for(k = 1; k < hs; k++) {
		if(!this->get_dev(k, &dev)) {
			dev->Run(crun);
		}
	}
	return 0;
}

