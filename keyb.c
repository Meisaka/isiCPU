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

static int Keyboard_MsgIn(struct isiInfo *info, struct isiInfo *host, uint16_t *msg, struct timespec mtime)
{
	struct LKBD* kyb;
	kyb = (struct LKBD*)info->rvstate;
	if(!kyb) return 0;
	switch(msg[0]) {
	case 2:
		break; // Handle HWI
	case 0x20E7:
		Keyboard_KDown(kyb, msg[1]);
		if(kyb->imsg) {
			if(info->hostcpu && info->hostcpu->MsgIn) {
				info->hostcpu->MsgIn(info->hostcpu, info, &kyb->imsg, mtime);
			} else {
				fprintf(stderr, "Keyboard Interrupt dropped!\n");
			}
		}
		return 0;
	case 0x20E8:
		Keyboard_KUp(kyb, msg[1]);
		if(kyb->imsg && info->hostcpu && info->hostcpu->MsgIn) {
			info->hostcpu->MsgIn(info->hostcpu, info, &kyb->imsg, mtime);
		}
		return 0;
	default:
		return 0; /* ignore other messages */
	}
	switch(msg[2]) {
	case 0: // Clear
		kyb->keykount = 0;
		break;
	case 1: // Get key pressed [C]
		msg[4] = Keyboard_get(kyb);
		break;
	case 2: // Is key [B] pressed?
		msg[4] = Keyboard_check(kyb, msg[3]);
		break;
	case 3: // Set interupt
		kyb->imsg = msg[3];
		break;
	default:
		break;
	}
	return 0;
}

int Keyboard_Init(struct isiInfo *info, const char *cfg)
{
	info->MsgIn = Keyboard_MsgIn;
	return 0;
}

