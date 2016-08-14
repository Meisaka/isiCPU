
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

static int speaker_Init(struct isiInfo *info);
struct isidcpudev speaker_Meta = {0x0001,0xc0f00001,0x5672746b};
struct isiConstruct speaker_Con = {
	.objtype = 0x5000,
	.name = "speaker",
	.desc = "2 Channel speaker",
	.PreInit = NULL, /* speaker_PreInit, */
	.Init = speaker_Init,
	.rvproto = &ISIREFNAME(struct speaker_rvstate),
	.meta = &speaker_Meta
};
void speaker_Register()
{
	isi_register(&speaker_Con);
}

static int speaker_Reset(struct isiInfo *info)
{
	struct speaker_rvstate *dev = (struct speaker_rvstate*)info->rvstate;
	dev->ch_a = 0;
	dev->ch_b = 0;
	isi_add_devsync(&info->id, 10000);
	return 0;
}

static int speaker_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	return 0;
}

static int speaker_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	struct speaker_rvstate *dev = (struct speaker_rvstate*)info->rvstate;
	switch(msg[0]) {
	case 0:
		dev->ch_a = msg[1];
		isi_resync_dev(&info->id);
		break;
	case 1:
		dev->ch_b = msg[1];
		isi_resync_dev(&info->id);
		break;
	}
	return 0;
}

static int speaker_MsgIn(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	switch(msg[0]) { /* message type, msg[1] is device index */
	case 0: return 0; /* CPU finished reset */
	case 1: return speaker_Query(info, src, msg+2, mtime); /* HWQ executed */
	case 2: return speaker_HWI(info, src, msg+2, mtime); /* HWI executed */
	default: break;
	}
	return 1;
}

static struct isiInfoCalls speaker_Calls = {
	.Reset = speaker_Reset, /* power on reset */
	.MsgIn = speaker_MsgIn, /* message from CPU or network */
};

static int speaker_Init(struct isiInfo *info)
{
	info->c = &speaker_Calls;
	return 0;
}

