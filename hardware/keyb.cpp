#include "../dcpuhw.h"

struct LKBD {
	uint16_t imsg;
	int keykount;
	unsigned char keybf[8];
	unsigned char keydown[8];

	void KDown(int kc);
	void KUp(int kc);
	int get();
	int check(int kc);
};

ISIREFLECT(struct LKBD,
	ISIR(LKBD, uint16_t, imsg)
	ISIR(LKBD, int, keykount)
	ISIR(LKBD, uint8_t, keybf)
	ISIR(LKBD, uint8_t, keydown)
)

class Keyboard : public isiInfo {
	virtual int MsgIn(struct isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int Reset();
};

static struct isidcpudev const Keyboard_Meta = {0x0001,0x30cf7406,MF_ECIV};
static isiClass<Keyboard> Keyboard_Con = {
	ISIT_HARDWARE, "keyboard", "Generic Keyboard",
	&ISIREFNAME(struct LKBD),
	NULL,
	NULL,
	&Keyboard_Meta
};
static struct isidcpudev const Keyboard_MetaTC = {0x0001,0x30c17406,MF_ECIV};
static isiClass<Keyboard> Keyboard_ConTC = {
	ISIT_HARDWARE, "tc_keyboard", "Generic Keyboard [TC]",
	&ISIREFNAME(struct LKBD),
	NULL,
	NULL,
	&Keyboard_MetaTC
};
void Keyboard_Register()
{
	isi_register(&Keyboard_Con);
	isi_register(&Keyboard_ConTC);
}

void LKBD::KDown(int kc)
{
	int i;
	for(i = 0; i < 8; i++) {
		if(!keydown[i]) { keydown[i] = kc; break; }
	}
	if(i==8) keydown[0] = kc;
	if(keykount < 8) {
		keybf[keykount++] = kc;
	} else {
		for(i = 0; i < 7; i++) { keybf[i] = keybf[i+1]; }
		keybf[7] = kc;
	}
}

int LKBD::get()
{
	int kc = keybf[0];
	int i;
	if(keykount) {
		for(i = 0; i < 7; i++) { keybf[i] = keybf[i+1]; }
		keykount--;
		return kc;
	}
	return 0;
}

void LKBD::KUp(int kc)
{
	for(int i = 0; i < 8; i++) {
		if(keydown[i] == kc) { keydown[i] = 0; }
	}
}

int LKBD::check(int kc)
{
	for(int i = 0; i < 8; i++) {
		if(keydown[i] == kc) {
			return 1;
		}
	}
	return 0;
}

int Keyboard::Reset()
{
	struct LKBD* kyb;
	kyb = (struct LKBD*)this->rvstate;
	kyb->imsg = 0;
	kyb->keykount = 0;
	for(int i = 0; i < 8; i++) { kyb->keydown[i] = 0; }
	return 0;
}

int Keyboard::MsgIn(struct isiInfo *host, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	struct LKBD* kyb;
	uint32_t iom[3];
	kyb = (struct LKBD*)this->rvstate;
	if(!kyb) return 0;
	switch(msg[0]) {
	case ISE_XINT:
		break; // Handle HWI
	case ISE_KEYDOWN:
		kyb->KDown(msg[1]);
		if(kyb->imsg) {
			iom[0] = ISE_XINT;
			iom[1] = 0;
			iom[2] = kyb->imsg;
			if(isi_message_dev(this, ISIAT_UP, iom, 3, mtime)) {
				isilog(L_DEBUG, "Keyboard Interrupt dropped!\n");
			}
		}
		return 0;
	case ISE_KEYUP:
		kyb->KUp(msg[1]);
		if(kyb->imsg) {
			iom[0] = ISE_XINT;
			iom[1] = 0;
			iom[2] = kyb->imsg;
			if(isi_message_dev(this, ISIAT_UP, iom, 3, mtime)) {
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
		msg[4] = kyb->get();
		break;
	case 2: // Is key [B] pressed?
		msg[4] = kyb->check(msg[3]);
		break;
	case 3: // Set interupt
		kyb->imsg = msg[3];
		break;
	default:
		break;
	}
	return 0;
}

