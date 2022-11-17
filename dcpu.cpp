
#include "dcpuhw.h"
#include "dcpuop.h"
#include "isitypes.h"
#include <stdio.h>
#include <string.h>

#define DCPUMODE_SKIP 1
#define DCPUMODE_INTQ 2
#define DCPUMODE_PROCINT 4
#define DCPUMODE_EXTINT 8

struct DCPU {
	uint16_t R[8];
	uint16_t PC;
	uint16_t SP;
	uint16_t EX;
	uint16_t IA;

	uint64_t cycl;
	int MODE;
	isiMemory* memptr;
	uint32_t hwcount;
	/* Interupt queue */
	int IQC;
	uint16_t IQU[256];
};

ISIREFLECT(struct DCPU,
	ISIR(DCPU, uint16_t, R)
	ISIR(DCPU, uint16_t, PC)
	ISIR(DCPU, uint16_t, SP)
	ISIR(DCPU, uint16_t, EX)
	ISIR(DCPU, uint16_t, IA)
	ISIR(DCPU, uint64_t, cycl)
	ISIR(DCPU, int, MODE)
	ISIR(DCPU, isiMemory*, memptr)
	ISIR(DCPU, uint32_t, hwcount)
	ISIR(DCPU, int, IQC)
)

#define HUGE_FIREBALL (-3141592)
#define BURNING (0xB19F14E)

static const char * DCPUOP[] = {
	"---", "SET", "ADD", "SUB", "MUL", "MLI", "DIV", "DVI",
	"MOD", "MDI", "AND", "BOR", "XOR", "SHR", "ASR", "SHL",
	"IFB", "IFC", "IFE", "IFN", "IFG", "IFA", "IFL", "IFU",
	"!18", "!19", "ADX", "SBX", "!1C", "!1D", "STI", "STD",
	"---", "JSR", "!02", "!03", "!04", "!05", "!06", "!07",
	"INT", "IAG", "IAS", "RFI", "IAQ", "!0d", "!0e", "!0f",
	"HWN", "HWQ", "HWI", "!13", "!14", "!15", "!16", "!17",
	"!18", "!19", "!1a", "!1b", "!1c", "!1d", "!1e", "!1f",
};
static const char * DCPUP[] = {
	"A", "B", "C", "X", "Y", "Z", "I", "J",
	"[A]", "[B]", "[C]", "[X]", "[Y]", "[Z]", "[I]", "[J]",
	"[A+%04x]", "[B+%04x]", "[C+%04x]", "[X+%04x]", "[Y+%04x]", "[Z+%04x]", "[I+%04x]", "[J+%04x]",
	"PSHPOP", "[SP]", "[SP+%04x]", "SP", "PC", "EX", "[%04x]", "%04x",
	"-1", " 0", " 1", " 2", " 3", " 4", " 5", " 6",
	" 7", " 8", " 9", "10", "11", "12", "13", "14",
	"15", "16", "17", "18", "19", "20", "21", "22",
	"23", "24", "25", "26", "27", "28", "29", "30",
};

static const char * DIAG_L4 =
	" A    B    C    X    Y    Z         \n"
	"%04x %04x %04x %04x %04x %04x       \n"
	" I    J    PC   SP   EX   IA   CL   \n"
	"%04x %04x %04x %04x %04x %04x % 2d  \n";
static const char * DIAG_L2 =
	" A    B    C    X    Y    Z    I    J    PC   SP   EX   IA   CyL \n"
	"%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x % 2d  >";

static inline void DCPU_reref(int, uint16_t, struct DCPU *, isiMemory*);
static inline void DCPU_rerefB(int, uint16_t, struct DCPU *, isiMemory*);
static inline uint16_t DCPU_deref(int, struct DCPU *, isiMemory*);
static inline uint16_t DCPU_derefB(int, struct DCPU *, isiMemory*);
static int DCPU_setonfire(struct DCPU *);
static inline void DCPU_skipref(int, struct DCPU *);

class DCPU16 : public isiCPUInfo {
public:
	DCPU16() {
		isi_setrate(this, 100000); // 100kHz
	}
	virtual int Run(isi_time_t crun);
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int QueryAttach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Attach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Reset();
protected:
	virtual int on_attached(int32_t to_point, isiInfo *dev, int32_t from_point);
};
static isiClass<DCPU16> DCPU_Con(
	ISIT_CPU, "dcpu", "DCPU-16 1.7",
	&ISIREFNAME(struct DCPU), NULL, NULL, NULL);

int DCPU16::QueryAttach(int32_t point, isiInfo *dev, int32_t devpoint) {
	if(!dev) return ISIERR_INVALIDPARAM;
	if(dev->otype == ISIT_MEM16) return 0;
	if(point == ISIAT_UP) return 0;
	return ISIERR_NOCOMPAT;
}
int DCPU16::Attach(int32_t point, isiInfo *dev, int32_t devpoint) {
	if(!dev) return ISIERR_INVALIDPARAM;
	return 0;
}
int DCPU16::on_attached(int32_t to_point, isiInfo *dev, int32_t from_point) {
	if(!dev) return ISIERR_INVALIDPARAM;
	return 0;
}

int DCPU16::Reset()
{
	struct DCPU *pr; pr = (struct DCPU*)this->rvstate;
	pr->memptr = this->mem;
	uint32_t i;
	for(i = 0; i < 8; i++)
	{
		pr->R[i] = 0;
	}
	if(pr->memptr)
		for(i = 0; i < 0x10000; i++) pr->memptr->d_wr(i, 0);
	else
		return -1;
	pr->PC = 0;
	pr->EX = 0;
	pr->SP = 0;
	pr->IA = 0;
	pr->MODE = 0;
	pr->cycl = 0;
	pr->IQC = 0;
	uint32_t iom[10] = {
		ISE_RESET, 0,
	};
	if(!isi_message_dev(this, ISIAT_UP, iom, 10, this->nrun)) {
		pr->hwcount = iom[1];
	}
	return 0;
}

void showdisasm_dcpu(const isiInfo *info)
{
	const struct DCPU * cpu = (const struct DCPU*)info->rvstate;
	uint16_t ma = cpu->PC;
	uint16_t m = cpu->memptr->i_rd(ma++);
	int op = m & 0x1f;
	int ob = (m >> 5) & 0x1f;
	int oa = (m >> 10) & 0x3f;
	int nwa, nwb, lu;
	uint16_t lua, lub;
	nwa = nwb = lua = lub = 0;
	lu = 0;
	if(oa >= 8 && oa < 16) {
		lu |= 1;
		lua = cpu->R[oa - 8];
	} else if((oa >= 16 && oa < 24) || oa == 26 || oa == 30 || oa == 31) {
		nwa = cpu->memptr->i_rd(ma++);
		if(oa != 31) lu |= 1;
		if(oa >= 16 && oa < 24) {
			lua = cpu->R[oa - 16] + nwa;
		} else if(oa == 24 || oa == 25) {
			lua = cpu->SP;
		} else if(oa == 26) {
			lua = cpu->SP + nwa;
		} else if(oa == 30) {
			lua = nwa;
		}
	}
	if(op) {
		if(ob >= 8 && ob < 16) {
			lu |= 2;
			lub = cpu->R[ob - 8];
		} else if((ob >= 16 && ob < 24) || ob == 26 || ob == 30 || ob == 31) {
			nwb = cpu->memptr->i_rd(ma++);
			if(ob != 31) lu |= 2;
			if(ob >= 16 && ob < 24) {
				lub = cpu->R[ob - 16] + nwb;
			} else if(ob == 24) {
				lub = cpu->SP - 1;
			} else if(ob == 25) {
				lub = cpu->SP;
			} else if(ob == 26) {
				lub = cpu->SP + nwb;
			} else if(ob == 30) {
				lub = nwb;
			}
		}
		fprintf(stderr, "%s ", DCPUOP[op]);
		fprintf(stderr, DCPUP[ob], nwb);
		fprintf(stderr, ", ");
		fprintf(stderr, DCPUP[oa], nwa);
	} else {
		fprintf(stderr, "%s ", DCPUOP[32+ob]);
		fprintf(stderr, DCPUP[oa], nwa);
	}
	if(lu & 2) {
		fprintf(stderr, "  b: %04x [%04x]", lub, cpu->memptr->i_rd(lub));
	}
	if(lu & 1) {
		fprintf(stderr, "  a: %04x [%04x]", lua, cpu->memptr->i_rd(lua));
	}
	fprintf(stderr, "\n");
}

void showdiag_dcpu(const isiInfo* info, int fmt)
{
	const DCPU16 *l_info = (const DCPU16 *)info;
	const struct DCPU * cpu = (const struct DCPU*)info->rvstate;
	const char *diagt;
	diagt = fmt ? DIAG_L2 : DIAG_L4;
	fprintf(stderr, diagt,
	cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3],cpu->R[4],cpu->R[5],
	cpu->R[6],cpu->R[7],cpu->PC,cpu->SP,cpu->EX,cpu->IA, l_info->cycl);
	if(fmt) {
		showdisasm_dcpu(info);
	}
}

int DCPU16::MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	struct DCPU *pr; pr = (struct DCPU*)this->rvstate;
	if(msg[0] == ISE_SREG) {
		for(int i = 1; i < 8 && i < len; i++) {
			pr->R[i - 1] = msg[i];
		}
		return 0;
	} else if(msg[0] == ISE_XINT) {
		if(pr->IA) {
			if(pr->IQC < 256) {
				pr->IQU[pr->IQC++] = msg[1];
				return 0;
			} else {
				return DCPU_setonfire(pr);
			}
		} else {
			return 0;
		}
	}
	return 0;
}

// Write referenced B operand
static inline void DCPU_reref(int p, uint16_t v, struct DCPU *pr, isiMemory *ram)
{
	switch(p & 0x1f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07: pr->R[p & 0x7] = v; return;
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f: ram->x_wr(pr->R[p & 0x7], v); return;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; ram->x_wr(ram->x_rd(pr->PC++) + pr->R[p & 0x7], v); return;
	case 0x18: ram->x_wr(--(pr->SP), v); return;
	case 0x19: ram->x_wr(pr->SP, v); return;
	case 0x1a: pr->cycl++; ram->x_wr(pr->SP + ram->x_rd(pr->PC++), v); return;
	case 0x1b: pr->SP = v; return;
	case 0x1c: pr->PC = v; return;
	case 0x1d: pr->EX = v; return;
	case 0x1e: pr->cycl++; ram->x_wr(ram->x_rd(pr->PC++), v); return;
	case 0x1f: pr->cycl++; pr->PC++; /* Fail silently */ return;
	default:
		isilog(L_DEBUG, "DCPU: Bad write: %x\n", p);
		// Should never actually happen
		return ;
	}
}

// re-Write referenced B operand
static inline void DCPU_rerefB(int p, uint16_t v, struct DCPU *pr, isiMemory *ram)
{
	switch(p & 0x1f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07: pr->R[p & 0x7] = v; return;
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f: ram->x_wr(pr->R[p & 0x7], v); return;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; ram->x_wr(ram->x_rd(pr->PC++) + pr->R[p & 0x7], v); return;
	case 0x18: ram->x_wr(pr->SP, v); return;
	case 0x19: ram->x_wr(pr->SP, v); return;
	case 0x1a: pr->cycl++; ram->x_wr(pr->SP + ram->x_rd(pr->PC++), v); return;
	case 0x1b: pr->SP = v; return;
	case 0x1c: pr->PC = v; return;
	case 0x1d: pr->EX = v; return;
	case 0x1e: pr->cycl++; ram->x_wr(ram->x_rd(pr->PC++), v); return;
	case 0x1f: pr->cycl++; pr->PC++; /* Fail silently */ return;
	default:
		isilog(L_DEBUG, "DCPU: Bad write: %x\n", p);
		// Should never actually happen
		return;
	}
}


// Dereference an A operand for ops
static inline uint16_t DCPU_deref(int p, struct DCPU *pr, isiMemory *ram)
{
	switch(p) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07: return pr->R[p & 0x7];
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f: return ram->x_rd(pr->R[p & 0x7]);
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; return ram->x_rd(ram->x_rd(pr->PC++) + pr->R[p & 0x7]);
	case 0x18: return ram->x_rd(pr->SP++);
	case 0x19: return ram->x_rd(pr->SP);
	case 0x1a: pr->cycl++; return ram->x_rd(pr->SP + ram->x_rd(pr->PC++));
	case 0x1b: return pr->SP;
	case 0x1c: return pr->PC;
	case 0x1d: return pr->EX;
	case 0x1e: pr->cycl++; return ram->x_rd(ram->x_rd(pr->PC++));
	case 0x1f: pr->cycl++; return ram->x_rd(pr->PC++);
	default:
		return (uint16_t)(p - 0x21); /* literals */
	}
}

// Dereference a B operand for R/W ops:
static inline uint16_t DCPU_derefB(int p, struct DCPU *pr, isiMemory *ram)
{
	switch(p) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07: return pr->R[p & 0x7];
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f: return ram->x_rd(pr->R[p & 0x7]);
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; return ram->x_rd(ram->x_rd(pr->PC) + pr->R[p & 0x7]);
	case 0x18: return ram->x_rd(--(pr->SP));
	case 0x19: return ram->x_rd(pr->SP);
	case 0x1a: pr->cycl++; return ram->x_rd(pr->SP + ram->x_rd(pr->PC));
	case 0x1b: return pr->SP;
	case 0x1c: return pr->PC;
	case 0x1d: return pr->EX;
	case 0x1e: pr->cycl++; return ram->x_rd(ram->x_rd(pr->PC));
	case 0x1f: pr->cycl++; return ram->x_rd(pr->PC);
	default:
		return (uint16_t)(p - 0x21); /* literals */
	}
}

// Skip references
static inline void DCPU_skipref(int p, struct DCPU* pr)
{
	if((p >= 0x10 && p < 0x18) || p == 0x1a || p == 0x1e || p == 0x1f) {
		pr->PC++;
		pr->cycl++;
	}
}


static const int DCPU_cycles1[] =
{
	0, 1, 2, 2, 2, 2, 3, 3,
	3, 3, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 2, 2, 2, 2
};

static const int DCPU_cycles2[] =
{
	0, 3, 0, 0, 0, 0, 0, 0,
	4, 1, 1, 3, 2, 0, 0, 0,
	2, 4, 4, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static const int DCPU_dtbl[] =
{
	0, 3, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,
	0, 0, 1, 1, 0, 0, 3, 3
};
static const int DCPU_dwtbl[] =
{
	0, 0, 1, 1, 1, 1, 2, 2,
	3, 3, 3, 3, 3, 2, 2, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 1, 0, 0, 0, 0
};

int DCPU16::Run(isi_time_t crun)
{
	struct DCPU *pr = (struct DCPU*)this->rvstate;
	isiMemory *ram = pr->memptr;
	size_t cycl = 0;
	uint32_t iom[10];
	int op;
	int oa;
	int ob;
	union {
		uint16_t u;
		int16_t s;
	} alu1;
	union {
		uint16_t u;
		uint32_t ui;
		int32_t si;
		int16_t s;
	} alu2;
	
	uintptr_t ccq = 0;
	if(this->ctl & ISICTL_STEP) {
		if(this->ctl & ISICTL_STEPE) {
			ccq = 1;
			this->ctl &= ~ISICTL_STEPE;
		} else {
			this->nrun = crun;
			isi_add_time(&this->nrun, this->runrate);
		}
	} else {
		ccq = this->rate;
	}
	while(ccq && isi_time_lt(&this->nrun, &crun)) {

	if(pr->MODE == BURNING) {
		cycl += 3;
	}
	if(this->ctl & ISICTL_DEBUG) {
		if(!(this->ctl & ISICTL_STEP) && !(this->ctl & ISICTL_STEPE) && ram->isbrk(pr->PC)) {
			this->ctl |= ISICTL_STEP | ISICTL_TRACE;
			isilog(L_DEBUG, "dcpu: break point at %04x\n", pr->PC);
			showdiag_dcpu(this, 1);
			break;
		}
		this->ctl &= ~ISICTL_STEPE;
	}

	if((pr->MODE & DCPUMODE_EXTINT)) {
		pr->MODE ^= DCPUMODE_EXTINT;
	} else {
		op = ram->x_rd(pr->PC++);
		oa = (op >> 10) & 0x003f;
		ob = (op >> 5) & 0x001f;
		op &= 0x001f;
		if(!op) {
			if(pr->MODE & DCPUMODE_SKIP) {
				DCPU_skipref(oa, pr);
				pr->MODE ^= DCPUMODE_SKIP;
				cycl ++;
				goto ecpu;
			}
			cycl += DCPU_cycles2[ob];
			// Special opcodes
			switch(ob) {
			case 0:
				// really special opcodes
				break;
			case SOP_JSR:
				alu1.u = DCPU_deref(oa, pr, ram);
				DCPU_reref(0x18, pr->PC, pr, ram);
				pr->PC = alu1.u;
				break;
			case 0x2: // UKN
			case 0x3: // UKN
			case 0x4: // UKN
			case 0x5: // UKN
			case 0x6: // UKN
			case SOP_HCF: // HCF
				alu1.u = DCPU_deref(oa, pr, ram);
				break;
				// 0x08 - 0x0f Interupts
			case SOP_INT:
				alu1.u = DCPU_deref(oa, pr, ram);
				if(pr->IA) {
					if(pr->IQC < 256) {
						pr->IQU[pr->IQC++] = alu1.u;
					} else {
						/* set on fire */
					}
				}
				break;
			case SOP_IAG:
				DCPU_reref(oa, pr->IA, pr, ram);
				break;
			case SOP_IAS:
				alu1.u = DCPU_deref(oa, pr, ram);
				if(!(pr->IA = alu1.u)) pr->IQC = 0;
				break;
			case SOP_RFI:
				alu1.u = DCPU_deref(oa, pr, ram);
				pr->R[0] = DCPU_deref(0x18, pr, ram);
				pr->PC = DCPU_deref(0x18, pr, ram);
				pr->MODE &= ~2;
				break;
			case SOP_IAQ:
				alu1.u = DCPU_deref(oa, pr, ram);
				if(alu1.u) {
					pr->MODE |= 2;
				} else {
					pr->MODE &= ~2;
				}
				break;
				// 0x10 - 0x17 Hardware control
			case SOP_HWN:
				DCPU_reref(oa, pr->hwcount, pr, ram);
				break;
			case SOP_HWQ:
				alu1.u = DCPU_deref(oa, pr, ram);
				iom[0] = ISE_QINT;
				iom[1] = alu1.u;
				iom[2] = pr->R[0];
				iom[3] = pr->R[1];
				iom[4] = pr->R[2];
				iom[5] = pr->R[3];
				iom[6] = pr->R[4];
				if(isi_message_dev(this, ISIAT_UP, iom, 10, this->nrun) == 0) {
					pr->R[0] = iom[2];
					pr->R[1] = iom[3];
					pr->R[2] = iom[4];
					pr->R[3] = iom[5];
					pr->R[4] = iom[6];
				} else {
					pr->R[0] = 0;
					pr->R[1] = 0;
					pr->R[2] = 0;
					pr->R[3] = 0;
					pr->R[4] = 0;
				}
				break;
			case SOP_HWI:
				alu1.u = DCPU_deref(oa, pr, ram);
				//pr->MODE |= DCPUMODE_EXTINT;
				iom[0] = ISE_XINT;
				iom[1] = alu1.u;
				iom[2] = pr->R[0];
				iom[3] = pr->R[1];
				iom[4] = pr->R[2];
				iom[5] = pr->R[3];
				iom[6] = pr->R[4];
				iom[7] = pr->R[5];
				iom[8] = pr->R[6];
				iom[9] = pr->R[7];
				if(0 != (op = isi_message_dev(this, ISIAT_UP, iom, 10, this->nrun))) {
					if(op > 0) {
						//pr->wcycl = op;
						pr->MODE |= DCPUMODE_EXTINT;
						pr->R[0] = iom[2];
						pr->R[1] = iom[3];
						pr->R[2] = iom[4];
						pr->R[3] = iom[5];
						pr->R[4] = iom[6];
						pr->R[5] = iom[7];
						pr->R[6] = iom[8];
						pr->R[7] = iom[9];
						goto ecpu;
					}
				}
				pr->R[0] = iom[2];
				pr->R[1] = iom[3];
				pr->R[2] = iom[4];
				pr->R[3] = iom[5];
				pr->R[4] = iom[6];
				pr->R[5] = iom[7];
				pr->R[6] = iom[8];
				pr->R[7] = iom[9];
				break;
			default:
				break;
			}
		} else {
			if(pr->MODE & DCPUMODE_SKIP) {
				DCPU_skipref(oa, pr);
				DCPU_skipref(ob, pr);
				if(op < OPN_IFMIN || op > OPN_IFMAX) {
					pr->MODE ^= DCPUMODE_SKIP;
				}
				cycl ++;
				goto ecpu;
			}
			alu1.u = DCPU_deref(oa, pr, ram);
			switch(DCPU_dtbl[op]) {
			case 1:
				alu2.ui = DCPU_derefB(ob, pr, ram) | (pr->EX << 16);
				break;
			case 2:
				alu2.u = DCPU_deref(ob, pr, ram);
				break;
			case 3:
				DCPU_reref(ob, alu1.u, pr, ram);
				break;
			default:
				break;
			}
			cycl += DCPU_cycles1[op];
			switch(op) {
			case OP_SET: // SET (WO)
				break;
			// Math functions:
			case OP_ADD: // ADD (R/W)
				alu2.ui = alu2.u + alu1.u;
				break;
			case OP_SUB: // SUB (R/W)
				alu2.ui = alu2.u - alu1.u;
				break;
			case OP_MUL: // MUL (R/W)
				alu2.ui = alu2.u * alu1.u;
				break;
			case OP_MLI: // MLI (R/W)
				alu2.si = alu2.s * alu1.s;
				break;
			case OP_DIV: // DIV (R/W)
				if(alu1.u) {
					alu2.ui = (alu2.ui << 16) / alu1.u;
				} else {
					alu2.ui = 0;
				}
				break;
			case OP_DVI: // DVI (R/W)
				alu2.ui = alu2.u << 16;
				{
					int sgn = (alu2.si < 0) ^ (alu1.s < 0);
					if(alu2.si < 0) {
						alu2.si = -alu2.si;
					}
					if(alu1.s < 0) {
						alu1.s = -alu1.s;
					}
					if(!alu1.u) {
						alu2.ui = 0;
					} else {
						alu2.ui = alu2.ui / alu1.u;
						if(sgn)
							alu2.si = -alu2.si;
					}
				}
				break;
			case OP_MOD: // MOD (R/W)
				if(alu1.u) {
					alu2.u = alu2.u % alu1.u;
				} else {
					alu2.u = 0;
				}
				break;
			case OP_MDI: // MDI (R/W)
				if(alu1.s) {
					alu2.s = alu2.s % alu1.s;
				} else {
					alu2.u = 0;
				}
				break;
			// Bits operations:
			case OP_AND: // AND (R/W)
				alu2.u &= alu1.u;
				break;
			case OP_BOR: // BOR (R/W)
				alu2.u |= alu1.u;
				break;
			case OP_XOR: // XOR (R/W)
				alu2.u ^= alu1.u;
				break;
			case OP_SHR: // SHR (R/W)
				alu2.ui = alu2.u << 16;
				alu2.ui >>= alu1.u;
				break;
			case OP_ASR: // ASR (R/W)
				alu2.ui = alu2.u << 16;
				alu2.si >>= alu1.u;
				break;
			case OP_SHL: // SHL (R/W)
				alu2.ui = alu2.u << alu1.u;
				break;
			// Conditional Operations:
			case OP_IFB: // IFB (RO)
				if(alu2.u & alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
					// fail
				}
				break;
			case OP_IFC: // IFC (RO)
				if(alu2.u & alu1.u) {
					// fail
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				} else {
				}
				break;
			case OP_IFE: // IFE (RO)
				if(alu2.u == alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFN: // IFN (RO)
				if(alu2.u != alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFG: // IFG (RO)
				if(alu2.u > alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFA: // IFA (RO)
				if(alu2.s > alu1.s) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFL: // IFL (RO)
				if(alu2.u < alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFU: // IFU (RO)
				if(alu2.s < alu1.s) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			// Composite operations:
			case 0x18: // UKN
				isilog(L_DEBUG, "DCPU: Invalid op: %x\n", op);
				break;
			case 0x19: // UKN
				isilog(L_DEBUG, "DCPU: Invalid op: (%04x) %x\n", pr->PC, op);
				break;
			case OP_ADX: // ADX (R/W)
				alu2.ui = alu2.u + alu1.u + pr->EX;
				break;
			case OP_SBX: // SBX (R/W)
				alu2.ui = alu2.u - alu1.u + pr->EX;
				break;
			case 0x1C: // UKN
				isilog(L_DEBUG, "DCPU: Invalid op: %x\n", op);
				break;
			case 0x1D: // UKN
				isilog(L_DEBUG, "DCPU: Invalid op: %x\n", op);
				break;
			case OP_STI: // STI (WO)
				pr->R[6]++; pr->R[7]++;
				break;
			case OP_STD: // STD (WO)
				pr->R[6]--; pr->R[7]--;
				break;
			default:
				break;
			}
			switch(DCPU_dwtbl[op]) {
			case 1:
				DCPU_rerefB(ob, alu2.u, pr, ram);
				pr->EX = alu2.ui >> 16;
				break;
			case 2:
				DCPU_rerefB(ob, alu2.ui >> 16, pr, ram);
				pr->EX = alu2.u;
				break;
			case 3:
				DCPU_rerefB(ob, alu2.u, pr, ram);
				break;
			}
		}
	}
	if(!(pr->MODE & (DCPUMODE_SKIP|DCPUMODE_INTQ)) && pr->IQC > 0) {
		DCPU_reref(0x18, pr->PC, pr, ram);
		DCPU_reref(0x18, pr->R[0], pr, ram);
		pr->R[0] = pr->IQU[0];
		pr->PC = pr->IA;
		pr->MODE |= DCPUMODE_INTQ;
		pr->IQC--;
		for(oa = pr->IQC; oa > 0; oa-- ) {
			pr->IQU[oa - 1] = pr->IQU[oa];
		}
	}
ecpu:
	cycl += pr->cycl;
	pr->cycl = 0;
	if(!cycl) cycl = 1;
	if(this->ctl & ISICTL_DEBUG) {
		if(this->ctl & ISICTL_RUNFOR) {
			if(this->rcycl > cycl) {
				this->rcycl -= cycl;
			} else {
				this->ctl &= ~ISICTL_RUNFOR;
				this->ctl |= ISICTL_STEP | ISICTL_TRACE;
				ccq = 0;
				showdiag_dcpu(this, 1);
				break;
			}
		}
	}
	if(ccq < cycl) { // used too many
		ccq = 0;
	} else {
		ccq -= cycl; // each op uses cycles
	}
	this->cycl += cycl;
	isi_add_time(&this->nrun, cycl * this->runrate);
	cycl = 0;
	} // while ccq
	return 0;
}

static int DCPU_setonfire(struct DCPU * pr)
{
	pr->MODE = BURNING;
	isilog(L_WARN, "DCPU: set on fire!\n");
	return HUGE_FIREBALL;
}

void DCPU_Register()
{
	isi_register(&DCPU_Con);
}

