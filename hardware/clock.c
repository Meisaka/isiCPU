
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

static int Clock_Init(struct isiInfo *info);
static struct isidcpudev Clock_Meta = {0x0001,0x12d0b402,MF_ECIV};
static struct isiConstruct Clock_Con = {
	.objtype = ISIT_HARDWARE,
	.name = "clock",
	.desc = "Generic Clock",
	.Init = Clock_Init,
	.rvproto = &ISIREFNAME(struct Clock_rvstate),
	.meta = &Clock_Meta
};
void Clock_Register()
{
	isi_register(&Clock_Con);
}

static int Clock_Reset(struct isiInfo *info)
{
	struct Clock_rvstate *clk = (struct Clock_rvstate*)info->rvstate;
	clk->rate = 0;
	clk->iword = 0;
	clk->accum = 0;
	clk->raccum = 0;
	clk->ctime = 0;
	return 0;
}

static int Clock_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, int len, struct timespec crun)
{
	struct Clock_rvstate *clk = (struct Clock_rvstate*)info->rvstate;
	switch(msg[0]) {
	case 0:
		if(!clk->rate && msg[1]) {
			info->nrun.tv_sec = crun.tv_sec;
			info->nrun.tv_nsec = crun.tv_nsec;
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

static int Clock_Tick(struct isiInfo *info, struct timespec crun)
{
	struct Clock_rvstate *clk = (struct Clock_rvstate*)info->rvstate;
	if(!clk->rate) return 0;
	if(!isi_time_lt(&info->nrun, &crun)) return 0;
	uint16_t iom[3];
	if(!(clk->raccum++ < clk->rate)) { /* handle divided rate */
		clk->raccum = 0;
		clk->ctime++;
		if(clk->iword) {
			iom[0] = ISE_XINT;
			iom[1] = 0;
			iom[2] = clk->iword;
			isi_message_dev(info, ISIAT_UP, iom, 3, info->nrun);
		}
	}
	if(clk->accum++ < 15) { /* magically sync the 60Hz base clock to 1 second */
		isi_addtime(&info->nrun, 16666666); /* tick */
	} else {
		isi_addtime(&info->nrun, 16666676); /* tock (occationally) */
		clk->accum = 0;
	}
	return 0;
}

static int Clock_MsgIn(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	switch(msg[0]) {
	case ISE_RESET: return 0;
	case ISE_QINT: return 0;
	case ISE_XINT:
		if(len < 10) return -1;
		return Clock_HWI(info, src, msg+2, len - 2, mtime);
	default: break;
	}
	return 1;
}

static struct isiInfoCalls ClockCalls = {
	.Reset = Clock_Reset,
	.MsgIn = Clock_MsgIn,
	.RunCycles = Clock_Tick
};

static int Clock_Init(struct isiInfo *info)
{
	info->c = &ClockCalls;
	return 0;
}

