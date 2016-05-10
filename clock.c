
#include "dcpuhw.h"

struct Clock_rvstate {
	uint16_t rate;
	uint8_t raccum;
	uint8_t accum;
	uint16_t iword;
	uint16_t ctime;
};

int Clock_SIZE(int t, const char *cfg)
{
	if(t == 0) return sizeof(struct Clock_rvstate);
	return 0;
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

static int Clock_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	struct Clock_rvstate *clk = (struct Clock_rvstate*)info->rvstate;
	switch(msg[0]) {
	case 0:
		if(!clk->rate && msg[1]) {
			info->nrun.tv_sec = crun.tv_sec;
			info->nrun.tv_nsec = crun.tv_nsec;
			fprintf(stderr, "Clock Reset\n");
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
	if(!(clk->raccum++ < clk->rate)) { /* handle divided rate */
		clk->raccum = 0;
		clk->ctime++;
		if(clk->iword && info->hostcpu && info->hostcpu->MsgIn) {
			info->hostcpu->MsgIn(info->hostcpu, info, &clk->iword, info->nrun);
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

static int Clock_MsgIn(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	switch(msg[0]) {
	case 0: return 0;
	case 1: return 0;
	case 2: return Clock_HWI(info, src, msg+2, mtime);
	default: break;
	}
	return 1;
}

int Clock_Init(struct isiInfo *info, const char *cfg)
{
	info->Reset = Clock_Reset;
	info->MsgIn = Clock_MsgIn;
	info->RunCycles = Clock_Tick;
	return 0;
}

