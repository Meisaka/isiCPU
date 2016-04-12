
#include <stdint.h>

struct imva_nvstate {
	uint16_t base; /* hw spec */
	uint16_t ovbase; /* hw spec */
	uint16_t ovoffset; /* hw spec */
	uint16_t colors; /* hw spec */
	uint16_t ovmode; /* hw spec */
	int blink; /* bool blink state  */
	uint32_t fgcolor;
	uint32_t bgcolor;
}

/* how we access memory */
#define IMVA_RD(m,a)  (m[a])

static void imva_colors(struct imva_nvstate *imva)
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

void imva_reset(struct imva_nvstate *imva)
{
	imva->base = 0;
	imva->ovbase = 0;
	imva->colors = 0x0fff;
	imva->ovmode = 0;
}

/* msg is assumed to point at registers A-J in order */
void imva_interrupt(struct imva_nvstate *imva, uint16_t *msg)
{
	switch(msg[0]) {
	case 0:
		imva->base = msg[1];
		break;
	case 1:
		imva->ovbase = msg[1];
		imva->ovoffset = msg[2];
		break;
	case 2:
		if(msg[1] != imva->colors) {
			imva->colors = msg[1];
			imva_colors(imva);
		}
		imva->ovmode = msg[2];
		break;
	case 0x0ffff:
		imva_reset(imva);
		break;
	default:
		break;
	}
}

/* imva is the device state
 * ram is the entire 64k words
 * rgba is a 320x200 RGBA pixel array (256000 bytes minimum)
 * slack is extra pixels to move to next line (0 if exactly sized buffer)
 */
int imva_raster(struct imva_nvstate *imva, uint16_t *ram, uint32_t *rgba, uint32_t slack)
{
	uint32_t bg, fg;
	uint16_t raddr, ova, ovo, ove;
	bg = imva->bgcolor;
	fg = imva->fgcolor;
	raddr = imva->base;
	if(!raddr) return 0; /* stand-by mode */

	int omode;
	omode = (imva->ovmode >> 4) & 3;
	uint32_t x, y, z, ovflag, cell, ovcell, of, v, vv;
	cell = 0;
	z = 0;
	ovflag = 0;
	of = 0;
	ova = imva->ovbase;
	ovo = raddr + imva->ovoffset;
	ove = raddr + (40*8); /* cell line words */
	if(!ova) ovo = 0xffff;
	for(y = 200; y--; of ^= 8) {
		z = 0;
		for(cell = 40; cell--; ) {
			v = (IMVA_RD(ram,raddr) >> of) & 0x00ff;
			if(raddr == ovo || z) {
				vv = IMVA_RD(ram,ova + z);
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

