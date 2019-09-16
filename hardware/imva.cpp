
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

class imva : public isiInfo {
public:
	virtual int MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int Reset();

	int interrupt(uint32_t *msg);
};
struct isidcpudev imva_Meta = {0x0538,0x75FEA113,0x59EA5742};
struct isiClass<imva> imva_Con(
	ISIT_HARDWARE, "imva", "IMVA Display",
	&ISIREFNAME(struct imva_rvstate),
	NULL,
	NULL,
	&imva_Meta);
void imva_Register()
{
	isi_register(&imva_Con);
}

int imva::Reset()
{
	struct imva_rvstate *imva = (struct imva_rvstate *)this->rvstate;
	imva->base = 0;
	imva->ovbase = 0;
	imva->colors = 0x0fff;
	imva->ovoffset = 0;
	imva->ovmode = 0;
	return 0;
}

/* msg is assumed to point at registers A-J in order */
int imva::interrupt(uint32_t *msg)
{
	struct imva_rvstate *imva = (struct imva_rvstate *)this->rvstate;
	struct memory64x16 *mem = (struct memory64x16*)this->mem;
	switch(msg[0]) {
	case 0:
		imva->base = msg[1];
		if(imva->base) {
			isi_add_devmemsync(this, mem, 50000000);
			isi_set_devmemsync_extent(this, mem, 0, imva->base, 4000);
		}
		break;
	case 1:
		imva->ovbase = msg[1];
		imva->ovoffset = msg[2];
		isi_set_devmemsync_extent(this, mem, 1, imva->ovbase, 16);
		break;
	case 2:
		if(msg[1] != imva->colors) {
			imva->colors = msg[1];
		}
		imva->ovmode = msg[2];
		isi_resync_dev(this);
		break;
	case 0x0ffff:
		this->Reset();
		isi_resync_dev(this);
		break;
	default:
		break;
	}
	return 0;
}

int imva::MsgIn(struct isiInfo *host, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	switch(msg[0]) {
	case 0: return this->Reset();
	case 1: return 0;
	case 2: return this->interrupt(msg+2);
	default: break;
	}
	return 0;
}

