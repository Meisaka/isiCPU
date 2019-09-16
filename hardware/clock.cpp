
#include "../dcpuhw.h"

struct Clock_rvstate {
	uint16_t rate;
	uint8_t raccum;
	uint8_t accum;
	uint16_t iword;
	uint16_t ctime;
};

ISIREFLECT(struct Clock_rvstate,
	ISIR(Clock_rvstate, uint16_t, rate)
	ISIR(Clock_rvstate, uint8_t, raccum)
	ISIR(Clock_rvstate, uint8_t, accum)
	ISIR(Clock_rvstate, uint16_t, iword)
	ISIR(Clock_rvstate, uint16_t, ctime)
)

class Clock : public isiInfo {
public:
	virtual int Run(isi_time_t crun);
	virtual int MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int Reset();

	int HWI(struct isiInfo *src, uint32_t *msg, int len, isi_time_t crun);
};

static struct isidcpudev Clock_Meta = {0x0001,0x12d0b402,MF_ECIV};
static isiClass<Clock> Clock_Con(
	ISIT_HARDWARE, "clock", "Generic Clock",
	&ISIREFNAME(struct Clock_rvstate),
	NULL,
	NULL,
	&Clock_Meta);

void Clock_Register()
{
	isi_register(&Clock_Con);
}

int Clock::Reset()
{
	struct Clock_rvstate *clk = (struct Clock_rvstate*)this->rvstate;
	clk->rate = 0;
	clk->iword = 0;
	clk->accum = 0;
	clk->raccum = 0;
	clk->ctime = 0;
	return 0;
}

int Clock::HWI(struct isiInfo *src, uint32_t *msg, int len, isi_time_t crun)
{
	struct Clock_rvstate *clk = (struct Clock_rvstate*)this->rvstate;
	switch(msg[0]) {
	case 0:
		if(!clk->rate && msg[1]) {
			this->nrun = crun;
			isilog(L_DEBUG, "Clock Reset\n");
			clk->accum = 0;
		}
		clk->rate = msg[1];
		clk->raccum = 0;
		break;
	case 1:
		msg[2] = clk->ctime;
		clk->ctime = 0;
		break;
	case 2:
		clk->iword = msg[1];
		break;
	}
	return 0;
}

int Clock::Run(isi_time_t crun)
{
	struct Clock_rvstate *clk = (struct Clock_rvstate*)this->rvstate;
	if(!clk->rate) return 0;
	if(!isi_time_lt(&this->nrun, &crun)) return 0;
	uint32_t iom[3];
	if(!(clk->raccum++ < clk->rate)) { /* handle divided rate */
		clk->raccum = 0;
		clk->ctime++;
		if(clk->iword) {
			iom[0] = ISE_XINT;
			iom[1] = 0;
			iom[2] = clk->iword;
			isi_message_dev(this, ISIAT_UP, iom, 3, this->nrun);
		}
	}
	if(clk->accum++ < 15) { /* magically sync the 60Hz base clock to 1 second */
		isi_add_time(&this->nrun, 16666666); /* tick */
	} else {
		isi_add_time(&this->nrun, 16666676); /* tock (occationally) */
		clk->accum = 0;
	}
	return 0;
}

int Clock::MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	switch(msg[0]) {
	case ISE_RESET: return 0;
	case ISE_QINT: return 0;
	case ISE_XINT:
		if(len < 10) return -1;
		return this->HWI(src, msg+2, len - 2, mtime);
	default: break;
	}
	return 1;
}

