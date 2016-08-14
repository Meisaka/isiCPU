#include "../dcpuhw.h"

struct LKBD {
	uint16_t imsg;
	int keykount;
	unsigned char keybf[8];
	unsigned char keydown[8];
};

ISIREFLECT(struct LKBD,
	ISIR(LKBD, uint16_t, imsg)
	ISIR(LKBD, int, keykount)
	ISIR(LKBD, uint8_t, keybf)
	ISIR(LKBD, uint8_t, keydown)
)

static int Keyboard_Init(struct isiInfo *info);
static struct isidcpudev Keyboard_Meta = {0x0001,0x30cf7406,MF_ECIV};
static struct isiConstruct Keyboard_Con = {
	.objtype = ISIT_HARDWARE,
	.name = "keyboard",
	.desc = "Generic Keyboard",
	.Init = Keyboard_Init,
	.rvproto = &ISIREFNAME(struct LKBD),
	.svproto = NULL,
	.meta = &Keyboard_Meta
};
void Keyboard_Register()
{
	isi_register(&Keyboard_Con);
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

static int Keyboard_MsgIn(struct isiInfo *info, struct isiInfo *host, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	struct LKBD* kyb;
	uint16_t iom[3];
	kyb = (struct LKBD*)info->rvstate;
	if(!kyb) return 0;
	switch(msg[0]) {
	case ISE_XINT:
		break; // Handle HWI
	case ISE_KEYDOWN:
		Keyboard_KDown(kyb, msg[1]);
		if(kyb->imsg) {
			iom[0] = ISE_XINT;
			iom[1] = 0;
			iom[2] = kyb->imsg;
			if(isi_message_dev(info, ISIAT_UP, iom, 3, mtime)) {
				isilog(L_DEBUG, "Keyboard Interrupt dropped!\n");
			}
		}
		return 0;
	case ISE_KEYUP:
		Keyboard_KUp(kyb, msg[1]);
		if(kyb->imsg) {
			iom[0] = ISE_XINT;
			iom[1] = 0;
			iom[2] = kyb->imsg;
			if(isi_message_dev(info, ISIAT_UP, iom, 3, mtime)) {
				isilog(L_DEBUG, "Keyboard Interrupt dropped!\n");
			}
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

static struct isiInfoCalls KeyboardCalls = {
	.MsgIn = Keyboard_MsgIn
};

static int Keyboard_Init(struct isiInfo *info)
{
	info->c = &KeyboardCalls;
	return 0;
}

