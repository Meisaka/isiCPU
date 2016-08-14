
#include "../dcpuhw.h"

struct imva_rvstate {
	uint16_t base; /* hw spec */
	uint16_t ovbase; /* hw spec */
	uint16_t ovoffset; /* hw spec */
	uint16_t colors; /* hw spec */
	uint16_t ovmode; /* hw spec */
	/* int blink; */ /* bool blink state  */
	/* uint32_t fgcolor; */
	/* uint32_t bgcolor; */
};
ISIREFLECT(struct imva_rvstate,
	ISIR(imva_rvstate, uint16_t, base)
	ISIR(imva_rvstate, uint16_t, ovbase)
	ISIR(imva_rvstate, uint16_t, ovoffset)
	ISIR(imva_rvstate, uint16_t, colors)
	ISIR(imva_rvstate, uint16_t, ovmode)
)

static int imva_Init(struct isiInfo *info);
struct isidcpudev imva_Meta = {0x0538,0x75FEA113,0x59EA5742};
struct isiConstruct imva_Con = {
	.objtype = 0x5000,
	.name = "imva",
	.desc = "IMVA Display",
	.Init = imva_Init,
	.rvproto = &ISIREFNAME(struct imva_rvstate),
	.meta = &imva_Meta
};
void imva_Register()
{
	isi_register(&imva_Con);
}

int imva_reset(struct isiInfo *info)
{
	struct imva_rvstate *imva = (struct imva_rvstate *)info->rvstate;
	imva->base = 0;
	imva->ovbase = 0;
	imva->colors = 0x0fff;
	imva->ovoffset = 0;
	imva->ovmode = 0;
	return 0;
}

/* msg is assumed to point at registers A-J in order */
int imva_interrupt(struct isiInfo *info, uint16_t *msg)
{
	struct imva_rvstate *imva = (struct imva_rvstate *)info->rvstate;
	struct memory64x16 *mem = (struct memory64x16*)info->mem;
	switch(msg[0]) {
	case 0:
		imva->base = msg[1];
		if(imva->base) {
			isi_add_devmemsync(&info->id, &mem->id, 50000000);
			isi_set_devmemsync_extent(&info->id, &mem->id, 0, imva->base, 4000);
		}
		break;
	case 1:
		imva->ovbase = msg[1];
		imva->ovoffset = msg[2];
		isi_set_devmemsync_extent(&info->id, &mem->id, 1, imva->ovbase, 16);
		break;
	case 2:
		if(msg[1] != imva->colors) {
			imva->colors = msg[1];
		}
		imva->ovmode = msg[2];
		isi_resync_dev(&info->id);
		break;
	case 0x0ffff:
		imva_reset(info);
		isi_resync_dev(&info->id);
		break;
	default:
		break;
	}
	return 0;
}

static int imva_MsgIn(struct isiInfo *info, struct isiInfo *host, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	switch(msg[0]) {
	case 0: return imva_reset(info);
	case 1: return 0;
	case 2: return imva_interrupt(info, msg+2);
	default: break;
	}
	return 0;
}

static struct isiInfoCalls imvaCalls = {
	.MsgIn = imva_MsgIn,
	.Reset = imva_reset
};

static int imva_Init(struct isiInfo *info)
{
	info->c = &imvaCalls;
	return 0;
}

