
#include "dcpuhw.h"

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

static int imva_Init(struct isiInfo *info, const uint8_t *cfg, size_t lcfg);
struct isidcpudev imva_Meta = {0x0538,0x75FEA113,0x59EA5742};
struct isiConstruct imva_Con = {
	0x5000, "imva", "IMVA Display",
	NULL, imva_Init, NULL,
	&ISIREFNAME(struct imva_rvstate), NULL,
	&imva_Meta
};
void imva_Register()
{
	isi_register(&imva_Con);
}

/* how we access memory */
#define IMVA_RD(m,a)  (m[a])

#if 0
/******* client size only ********/
static void imva_colors(struct imva_rvstate *imva)
{
	uint8_t r,g,b,c;
	uint32_t fg, bg;
	fg = imva->colors & 0xfff;
	c = (imva->colors & 0xf000) >> 9;
	b = ((fg & 0xf00) >> 4 | (fg & 0xf00) >> 8);
	g = ((fg & 0x0f0) >> 4 | (fg & 0x0f0));
	r = ((fg & 0x00f) << 4 | (fg & 0x00f));
	bg = ((r >> 5) + c) | (((g >> 5) + c) << 8) | (((b >> 5) + c) << 16);
	if(c > r) r = c + 15; else r -= c;
	if(c > g) g = c + 15; else g -= c;
	if(c > b) b = c + 15; else b -= c;
	fg = r | (g << 8) | (b << 16);
	imva->fgcolor = fg;
	imva->bgcolor = bg;
}
#endif

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

static int imva_MsgIn(struct isiInfo *info, struct isiInfo *host, uint16_t *msg, struct timespec mtime)
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

static int imva_Init(struct isiInfo *info, const uint8_t * cfg, size_t lcfg)
{
	info->c = &imvaCalls;
	return 0;
}

#if 0
/******* client size only ********/
/* imva is the device state
 * ram is the entire 64k words
 * rgba is a 320x200 RGBA pixel array (256000 bytes minimum)
 * slack is extra pixels to move to next line (0 if exactly sized buffer)
 */
int imva_raster(struct imva_rvstate *imva, uint16_t *ram, uint32_t *rgba, uint32_t slack)
{
	uint32_t bg, fg;
	uint16_t raddr, ova, ovo, ove;
	bg = imva->bgcolor;
	fg = imva->fgcolor;
	raddr = imva->base;
	if(!raddr) return 0; /* stand-by mode */

	int omode;
	omode = (imva->ovmode >> 4) & 3;
	uint32_t x, y, z, ovflag, cell, of, v, vv;
	cell = 0;
	z = 0;
	ovflag = 0;
	of = 0;
	ova = imva->ovbase;
	ovo = raddr + imva->ovoffset;
	ove = ovo + (40*8); /* cell line words */
	if(!ova || imva->blink) ovo = raddr - 1;
	for(y = 200; y--; of ^= 8) {
		z = 0;
		for(cell = 40; cell--; ) {
			v = (IMVA_RD(ram,raddr) >> of) & 0x00ff;
			if(raddr == ovo || z) {
				vv = IMVA_RD(ram,ova + z) >> of;
				ovflag = 1;
				z ^= 1;
				switch(omode) {
				case 0: v |= vv; break;
				case 1: v ^= vv; break;
				case 2: v &= vv; break;
				case 3: v = vv; break;
				}
			}
			for(x = 8; x--; *(rgba++) = (v & (1 << (x & 7))) ? fg : bg);
			raddr++;
		}
		rgba += slack;
		if(!of) {
			raddr -= 40;
		} else {
			if(ovflag) {
				ova += 2;
				ovflag = 0;
				if(ovo != ove) ovo += 40;
			}
		}
	}
	return 1;
}
#endif

