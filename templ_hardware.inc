
#include "dcpuhw.h"

/* state while this device exists/active (runtime volatile) */
struct ${NAME}_rvstate {
};

/* state while running on this server (server volatile) */
struct ${NAME}_svstate {
};

int ${NAME}_SIZE(int t, const char *cfg)
{
	if(t == 0) return sizeof(struct ${NAME}_rvstate);
	if(t == 1) return sizeof(struct ${NAME}_svstate);
	return 0;
}

static int ${NAME}_Reset(struct isiInfo *info)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	return 0;
}

static int ${NAME}_OnReset(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	return 0;
}

static int ${NAME}_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	return 0;
}

static int ${NAME}_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	switch(msg[0]) {
	case 0:
		break;
	}
	return 0;
}

static int ${NAME}_Tick(struct isiInfo *info, struct timespec crun)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	// if(!isi_time_lt(&info->nrun, &crun)) return 0; /* wait for scheduled time */
	return 0;
}

static int ${NAME}_MsgIn(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case 0: return ${NAME}_OnReset(info, src, msg+2, mtime); /* CPU finished reset */
	case 1: return ${NAME}_Query(info, src, msg+2, mtime); /* HWQ executed */
	case 2: return ${NAME}_HWI(info, src, msg+2, mtime); /* HWI executed */
	default: break;
	}
	return 1;
}

int ${NAME}_Init(struct isiInfo *info, const char *cfg)
{
	info->Reset = ${NAME}_Reset; /* power on reset */
	info->MsgIn = ${NAME}_MsgIn; /* message from CPU or network */
	info->RunCycles = ${NAME}_Tick; /* scheduled runtime */
	return 0;
}

