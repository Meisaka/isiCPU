#include "dcpuhw.h"

struct LKBD {
	uint16_t imsg;
	int keykount;
	unsigned char keybf[8];
	unsigned char keydown[8];
};

int Keyboard_SIZE(int t, const char *cfg)
{
	switch(t) {
	case 0: return sizeof(struct LKBD);
	default: return 0;
	}
}

static void Keyboard_KDown(struct LKBD *kb, int kc)
{
	int i;
	for(i = 0; i < 8; i++) {
		if(!kb->keydown[i]) {
			kb->keydown[i] = kc;
			break;
		}
	}
	if(i==8) kb->keydown[0] = kc;
	if(kb->keykount < 8) {
		kb->keybf[kb->keykount++] = kc;
	} else {
		for(i = 0; i < 7; i++) {
			kb->keybf[i] = kb->keybf[i+1];
		}
		kb->keybf[7] = kc;
	}
}

static int Keyboard_get(struct LKBD *kb)
{
	int kc = kb->keybf[0];
	int i;
	if(kb->keykount) {
		for(i = 0; i < 7; i++) {
			kb->keybf[i] = kb->keybf[i+1];
		}
		kb->keykount--;
		return kc;
	}
	return 0;
}

static int Keyboard_check(struct LKBD *kb, int kc)
{
	int i;
	for(i = 0; i < 8; i++) {
		if(kb->keydown[i] == kc) {
			return 1;
		}
	}
	return 0;
}

static void Keyboard_KUp(struct LKBD *kb, int kc)
{
	int i;
	for(i = 0; i < 8; i++) {
		if(kb->keydown[i] == kc) {
			kb->keydown[i] = 0;
		}
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
		msg[2] = Keyboard_get(kyb);
		break;
	case 2: // Is key [B] pressed?
		msg[2] = Keyboard_check(kyb, msg[1]);
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
			Keyboard_KDown(kyb, tbf[2]);
			if(kyb->imsg) {
				isi->msg = kyb->imsg;
				return 1;
			}
		}
		if(tbf[0] == 0x080 && tbf[1] == 0x0E8) {
			Keyboard_KUp(kyb, tbf[2]);
			if(kyb->imsg) {
				isi->msg = kyb->imsg;
				return 1;
			}
		}
	}
	return 0;
}


