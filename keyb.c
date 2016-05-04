#include "dcpuhw.h"

struct LKBD {
	uint16_t imsg;
	int keykount;
	unsigned char keybf[8];
};

int Keyboard_SIZE(int t, const char *cfg)
{
	switch(t) {
	case 0: return sizeof(struct LKBD);
	default: return 0;
	}
}

int Keyboard_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	struct LKBD* kyb;
	kyb = (struct LKBD*)info->rvstate;
	if(!kyb) return 0;
	switch(msg[0]) {
	case 0: // Clear
		kyb->keykount = 0;
		break;
	case 1: // Get key pressed [C]
		if(kyb->keykount) { // TODO buffer keys
			msg[2] = kyb->keybf[0];
			kyb->keykount--;
		} else {
			msg[2] = 0;
		}
		break;
	case 2: // Is key [B] pressed?
		if(kyb->keykount) { // XXX should scan all pressed keys
			msg[2] = ((kyb->keybf[0] == msg[1]) ? 1 : 0);
		} else {
			msg[2] = 0;
		}
		break;
	case 3: // Set interupt
		kyb->imsg = msg[1];
		break;
	default:
		break;
	}
	return 0;
}

int Keyboard_Tick(struct isiInfo *info, struct systemhwstate *isi, struct timespec crun)
{
	struct timeval ltv;
	fd_set fds;
	int i;
	unsigned char tbf[4];
	struct LKBD* kyb;
	kyb = (struct LKBD*)info->rvstate;
	ltv.tv_sec = 0;
	ltv.tv_usec = 0;
	if(!isi->net) return 0;
	FD_ZERO(&fds);
	FD_SET(isi->net->sfd, &fds);
	i = select(isi->net->sfd+1, &fds, NULL, NULL, &ltv);
	if(i > 0) {
		i = recv(isi->net->sfd, tbf, 3, 0);
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


