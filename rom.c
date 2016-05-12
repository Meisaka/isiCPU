
#include "dcpuhw.h"

struct EEROM_rvstate {
	uint16_t sz;
};

int EEROM_SIZE(int t, const char *cfg)
{
	const char * cp = strchr(cfg, ':');
	unsigned int rqs = 0;
	rqs = 0;
	if(cp) {
		sscanf(cp, ":size=%u", &rqs);
	}
	switch(t) {
	case 0: return sizeof(struct EEROM_rvstate) + rqs;
	default: return 0;
	}
}

static int EEROM_Reset(struct isiInfo *info, struct isiInfo *host, struct timespec mtime)
{
	if(!info->mem) return 1;
	if(!info->rvstate) return 1;
	memcpy(
		((isiram16)info->mem)->ram,
		((struct EEROM_rvstate *)info->rvstate)+1,
		((struct EEROM_rvstate *)info->rvstate)->sz << 1
	      );
	return 0;
}

static int EEROM_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	msg[2] = ((struct EEROM_rvstate *)info->rvstate)->sz;
	return 0;
}

static int EEROM_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	switch(msg[0]) {
	case 0:
		memcpy(
			((isiram16)info->mem)->ram,
			((struct EEROM_rvstate *)info->rvstate)+1,
			((struct EEROM_rvstate *)info->rvstate)->sz << 1
		      );
		break;
	case 1:
		break;
	}
	return 0;
}

static int EEROM_MsgIn(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	switch(msg[0]) {
	case 0: return EEROM_Reset(info, src, mtime);
	case 1: return EEROM_Query(info, src, msg+2, mtime);
	case 2: return EEROM_HWI(info, src, msg+2, mtime);
	default: break;
	}
	return 1;
}

int EEROM_Init(struct isiInfo *info, const char * cfg)
{
	info->MsgIn = EEROM_MsgIn;
	return 0;
}

