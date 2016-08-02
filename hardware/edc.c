
#include "../dcpuhw.h"

#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF(A...) fprintf(stderr, A)
#endif

// controls actual RAM addressing
#define LINE_COLUMNS 128
#define LINE_COUNT 4

struct EDC_Dev {
	unsigned lines;
	unsigned columns;
	int backlight;
	uint32_t status;
	uint32_t address; // of cursor
	uint16_t version;
	uint8_t vram[LINE_COUNT * LINE_COLUMNS];
	uint16_t gram[512];
};

size_t EDC_SIZE()
{
	return sizeof(struct EDC_Dev);
}

int EDC_Init(struct EDC_Dev *dsp, unsigned w, unsigned h)
{
	if(h > 4 || h == 0) return 1;
	if(w & 3 || w > 124) return 1;
	dsp->lines = h;
	dsp->columns = w ^ (w & 3);
	dsp->backlight = 0;
	dsp->status = 0;
	dsp->address = 0;
	dsp->version = 0x0500 | ((h - 1) << 5) | ((h >> 2) & 31);
	int i;
	for(i = 0; i < 256; i++) dsp->vram[i] = 32;
	for(i = 0; i < 512; i++) dsp->gram[i] = 0;
	return 0;
}

int EDC_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	struct EDC_Dev *dsp;
	if(!info) return -1;
	dsp = (struct EDC_Dev*)info->rvstate;

	DEBUG_PRINTF("EDC - HWQ\n");
	msg[0] = 0xe4ff;
	msg[1] = 0xed73;
	msg[2] = dsp->version;
	msg[3] = 0x5742;
	msg[4] = 0x59ea;

	return 0;
}

int EDC_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	struct EDC_Dev *dsp;
	int i;

	if(!info) return -1;
	dsp = (struct EDC_Dev*)info->rvstate;
	uint16_t a = msg[1];

	switch(msg[0]) {
	case 0:
		DEBUG_PRINTF("EDC - HWI: control %04x\n", a);
		dsp->status &= ~0x1f;
		dsp->status |= a & 0x1f;
		dsp->backlight = (dsp->status & 3) == 3 ? -1 : 0;
		break;
	case 1:
		DEBUG_PRINTF("EDC - HWI: set-address %04x\n", a);
		if(a < 0x0400) {
			dsp->address = a;
		}
		break;
	case 2:
		DEBUG_PRINTF("EDC - HWI: reset-control %04x\n", a);
		if(a & 1) {
			for(i = 0; i < 256; i++) dsp->vram[i] = 32;
		}
		if(a & 2) {
			dsp->address = 0;
		}
		break;
	case 3:
		if(dsp->address > 0x400) {
			dsp->address = 0;
			DEBUG_PRINTF("EDC - HWI: write out of bound\n");
		}
		if(dsp->address > 0x200) {
			DEBUG_PRINTF("EDC - HWI: write glyph ram %04x\n", a);
			dsp->gram[dsp->address++ - 0x200] = a;
		} else {
			uint32_t line, col;
			line = dsp->address >> 7;
			col = dsp->address & 0x7f;
			DEBUG_PRINTF("EDC - HWI: write line=%d,col=%d : %04x\n", line, col, a);
			if(col >= dsp->columns) {
				line++;
				if(line >= dsp->lines) {
					col = 0;
					line = 0;
				}
			} else if(line >= dsp->lines) {
				col = 0;
				line = 0;
			} else {
				dsp->vram[LINE_COLUMNS*line+col] = (uint8_t)(a & 0xff);
				col++;
				if(col >= dsp->columns) {
					col = 0;
					line++;
					if(line >= dsp->lines) {
						line = 0;
					}
				}
			}
			dsp->address = (line << 7) | col;
		}
		if(dsp->address > 0x400) {
			dsp->address = 0;
		}
		break;
	case 0xffff:
		DEBUG_PRINTF("EDC - HWI: full-reset\n");
		for(i = 0; i < 256; i++) dsp->vram[i] = 0;
		for(i = 0; i < 512; i++) dsp->gram[i] = 0;
		break;
	default:
		DEBUG_PRINTF("EDC - HWI: Unsupported command: %04x\n", msg[0]);
		break;
	}
	return 0;
}

int EDC_MsgIn(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	switch(msg[0]) {
	case 0: break;
	case 1: return EDC_Query(info,src,msg+2,mtime);
	case 2: return EDC_HWI(info,src,msg+2,mtime);
	default: break;
	}
	return 0;
}

