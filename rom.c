
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

int EEROM_Reset(struct isiInfo *info, struct isiInfo *host, struct timespec mtime)
{
	memcpy(
		((isiram16)((struct isiCPUInfo *)host)->mem)->ram,
		((struct EEROM_rvstate *)info->rvstate)+1,
		((struct EEROM_rvstate *)info->rvstate)->sz
	      );
	return 0;
}
int EEROM_Init(struct isiInfo *info, const char * cfg)
{
	return 0;
}

int EEROM_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	msg[2] = ((struct EEROM_rvstate *)info->rvstate)->sz;
	return HWQ_SUCCESS;
}

int EEROM_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	switch(msg[0]) {
	case 0:
		break;
	case 1:
		break;
	}
	return 0;
}

