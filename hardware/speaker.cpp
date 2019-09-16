
#include "../dcpuhw.h"

/* state while this device exists/active (runtime volatile) */
struct speaker_rvstate {
	uint16_t ch_a;
	uint16_t ch_b;
};
ISIREFLECT(struct speaker_rvstate,
	ISIR(speaker_rvstate, uint16_t, ch_a)
	ISIR(speaker_rvstate, uint16_t, ch_b)
)

class speaker : public isiInfo {
public:
	virtual int MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int Reset();

	int HWQ(struct isiInfo *src, uint32_t *msg, isi_time_t mtime);
	int HWI(struct isiInfo *src, uint32_t *msg, isi_time_t crun);
};

struct isidcpudev speaker_Meta = {0x0001,0xc0f00001,0x5672746b};
struct isiClass<speaker> speaker_Con(
	ISIT_HARDWARE, "speaker", "2 Channel speaker",
	&ISIREFNAME(struct speaker_rvstate),
	NULL,
	NULL,
	&speaker_Meta
);
void speaker_Register()
{
	isi_register(&speaker_Con);
}

int speaker::Reset()
{
	struct speaker_rvstate *dev = (struct speaker_rvstate*)this->rvstate;
	dev->ch_a = 0;
	dev->ch_b = 0;
	isi_add_devsync(this, 10000);
	return 0;
}

int speaker::HWQ(struct isiInfo *src, uint32_t *msg, isi_time_t mtime)
{
	return 0;
}

int speaker::HWI(struct isiInfo *src, uint32_t *msg, isi_time_t crun)
{
	struct speaker_rvstate *dev = (struct speaker_rvstate*)this->rvstate;
	switch(msg[0]) {
	case 0:
		dev->ch_a = msg[1];
		isi_resync_dev(this);
		break;
	case 1:
		dev->ch_b = msg[1];
		isi_resync_dev(this);
		break;
	}
	return 0;
}

int speaker::MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	switch(msg[0]) { /* message type, msg[1] is device index */
	case 0: return 0; /* CPU finished reset */
	case 1: return this->HWQ(src, msg+2, mtime); /* HWQ executed */
	case 2: return this->HWI(src, msg+2, mtime); /* HWI executed */
	default: break;
	}
	return 1;
}

