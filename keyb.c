#include "dcpuhw.h"

struct LKBD {
	uint16_t imsg;
	int keykount;
	unsigned char keybf[8];
};

int Keyboard_SIZE()
{
	return sizeof(struct LKBD);
}

int Keyboard_Query(void *hwd,struct systemhwstate * isi)
{
	isi->regs[0] = 0x7406;
	isi->regs[1] = 0x30cf;
	isi->regs[2] = 1;
	isi->regs[3] = ECIV_LO;
	isi->regs[4] = ECIV_HI;
	return HWQ_SUCCESS;
}

int Keyboard_HWI(void *hwd, struct systemhwstate *isi)
{
	struct LKBD* kyb;
	kyb = (struct LKBD*)hwd;
	if(!kyb) return 0;
	switch(isi->regs[0]) {
	case 0: // Clear
		kyb->keykount = 0;
		break;
	case 1: // Get key pressed [C]
		if(kyb->keykount) { // TODO buffer keys
			isi->regs[2] = kyb->keybf[0];
			kyb->keykount--;
		} else {
			isi->regs[2] = 0;
		}
		break;
	case 2: // Is key [B] pressed?
		if(kyb->keykount) { // XXX should scan all pressed keys
			isi->regs[2] = ((kyb->keybf[0] == isi->regs[1]) ? 1 : 0);
		} else {
			isi->regs[2] = 0;
		}
		break;
	case 3: // Set interupt
		kyb->imsg = isi->regs[1];
	fprintf(stderr, "KEYB: IntOn %04x \n", isi->regs[1]);
		break;
	default:
		break;
	}
	return 0;
}

int Keyboard_Tick(void *hwd, struct systemhwstate *isi)
{
	struct timeval ltv;
	fd_set fds;
	int i,k;
	unsigned char tbf[4];
	struct LKBD* kyb;
	kyb = (struct LKBD*)hwd;
	ltv.tv_sec = 0;
	ltv.tv_usec = 0;
	if(!isi->netwfd) return 0;
	FD_ZERO(&fds);
	FD_SET(isi->netwfd, &fds);
	i = select(isi->netwfd+1, &fds, NULL, NULL, &ltv);
	if(i > 0) {
		i = recv(isi->netwfd, tbf, 3, 0);
		if(tbf[0] == 0x080 && tbf[1] == 0x0E7) {
			kyb->keybf[0] = tbf[2];
			kyb->keykount = 1;
			if(kyb->imsg) {
				isi->msg = kyb->imsg;
				return 1;
			}
		}
	}
	return 0;
}


