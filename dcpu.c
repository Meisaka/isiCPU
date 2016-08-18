
#include "dcpuhw.h"
#include "opcode.h"
#include "cputypes.h"
#include <stdio.h>
#include <string.h>

#define DCPUMODE_SKIP 1
#define DCPUMODE_INTQ 2
#define DCPUMODE_PROCINT 4
#define DCPUMODE_EXTINT 8

struct DCPU {
	uint16_t msg;
	uint16_t dai;
	uint16_t R[8];
	uint16_t PC;
	uint16_t SP;
	uint16_t EX;
	uint16_t IA;

	uint64_t cycl;
	int MODE;
	isiram16 memptr;
	uint16_t hwcount;
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
	ISIR(DCPU, isiram16, memptr)
	ISIR(DCPU, uint16_t, hwcount)
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
	" A    B    C    X    Y    Z   \n"
	"%04x %04x %04x %04x %04x %04x \n"
	" I    J    PC   SP   EX   IA   CL\n"
	"%04x %04x %04x %04x %04x %04x % 2d \n";
static const char * DIAG_L2 =
	" A    B    C    X    Y    Z    I    J    PC   SP   EX   IA   CyL \n"
	"%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x % 2d  >";

static inline void DCPU_reref(int, uint16_t, struct DCPU *, isiram16);
static inline void DCPU_rerefB(int, uint16_t, struct DCPU *, isiram16);
static inline uint16_t DCPU_deref(int, struct DCPU *, isiram16);
static inline uint16_t DCPU_derefB(int, struct DCPU *, isiram16);
static int DCPU_setonfire(struct DCPU *);
static inline void DCPU_skipref(int, struct DCPU *);
static int DCPU_reset(struct isiInfo *);
static int DCPU_interrupt(struct isiInfo *, struct isiInfo *, int32_t lsindex, uint16_t *, int, struct timespec);
static int DCPU_run(struct isiInfo *, struct timespec);
static int DCPU_init(struct isiInfo *info);

static struct isiConstruct DCPU_Con = {
	.objtype = ISIT_DCPU,
	.name = "dcpu",
	.desc = "DCPU-16 1.7",
	.Init = DCPU_init,
	.rvproto = &ISIREFNAME(struct DCPU),
};

void DCPU_Register()
{
	isi_register(&DCPU_Con);
}

static int DCPU_QueryAttach(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!info || !dev) return ISIERR_INVALIDPARAM;
	if(dev->id.objtype == ISIT_MEM6416) return 0;
	if(point == ISIAT_UP) return 0;
	return ISIERR_NOCOMPAT;
}
static int DCPU_Attach(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!info || !dev) return ISIERR_INVALIDPARAM;
	return 0;
}
static int DCPU_Attached(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!info || !dev) return ISIERR_INVALIDPARAM;
	return 0;
}

static struct isiInfoCalls DCPUCalls = {
	.RunCycles = DCPU_run,
	.Reset = DCPU_reset,
	.MsgIn = DCPU_interrupt,
	.QueryAttach = DCPU_QueryAttach,
	.Attach = DCPU_Attach,
	.Attached = DCPU_Attached
};

static int DCPU_init(struct isiInfo *info)
{
	isi_setrate((struct isiCPUInfo *)info, 100000); // 100kHz
	info->c = &DCPUCalls;
	return 0;
}

static int DCPU_reset(struct isiInfo *info)
{
	struct DCPU *pr; pr = (struct DCPU*)info->rvstate;
	pr->memptr = (isiram16)info->mem;
	int i;
	for(i = 0; i < 8; i++)
	{
		pr->R[i] = 0;
	}
	if(pr->memptr)
		for(i = 0; i < 0x10000; i++) isi_cpu_wrmem(pr->memptr, (uint16_t)i, 0);
	else
		return -1;
	pr->PC = 0;
	pr->EX = 0;
	pr->SP = 0;
	pr->IA = 0;
	pr->MODE = 0;
	pr->cycl = 0;
	pr->IQC = 0;
	pr->msg = ISE_RESET;
	if(!isi_message_dev(info, ISIAT_UP, &pr->msg, 10, info->nrun)) {
		pr->hwcount = pr->dai;
	}
	return 0;
}

void showdisasm_dcpu(const struct isiInfo *info)
{
	const struct DCPU * cpu = (const struct DCPU*)info->rvstate;
	uint16_t ma = cpu->PC;
	uint16_t m = (cpu->memptr)->ram[ma++];
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
		nwa = (cpu->memptr)->ram[ma++];
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
			nwb = (cpu->memptr)->ram[ma++];
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
		fprintf(stderr, "  b: %04x [%04x]", lub, cpu->memptr->ram[lub]);
	}
	if(lu & 1) {
		fprintf(stderr, "  a: %04x [%04x]", lua, cpu->memptr->ram[lua]);
	}
	fprintf(stderr, "\n");
}

void showdiag_dcpu(const struct isiInfo* info, int fmt)
{
	const struct isiCPUInfo *l_info = (const struct isiCPUInfo *)info;
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

static int DCPU_interrupt(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	struct DCPU *pr; pr = (struct DCPU*)info->rvstate;
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
static inline void DCPU_reref(int p, uint16_t v, struct DCPU* pr, isiram16 ram)
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
	case 0x0f: isi_cpu_wrmem(ram, pr->R[p & 0x7], v); return;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; isi_cpu_wrmem(ram, isi_cpu_rdmem(ram, pr->PC++) + pr->R[p & 0x7], v); return;
	case 0x18: isi_cpu_wrmem(ram, --(pr->SP), v); return;
	case 0x19: isi_cpu_wrmem(ram, pr->SP, v); return;
	case 0x1a: pr->cycl++; isi_cpu_wrmem(ram, pr->SP + isi_cpu_rdmem(ram,pr->PC++), v); return;
	case 0x1b: pr->SP = v; return;
	case 0x1c: pr->PC = v; return;
	case 0x1d: pr->EX = v; return;
	case 0x1e: pr->cycl++; isi_cpu_wrmem(ram, isi_cpu_rdmem(ram,pr->PC++), v); return;
	case 0x1f: pr->cycl++; pr->PC++; /* Fail silently */ return;
	default:
		isilog(L_DEBUG, "DCPU: Bad write: %x\n", p);
		// Should never actually happen
		return ;
	}
}

// re-Write referenced B operand
static inline void DCPU_rerefB(int p, uint16_t v, struct DCPU* pr, isiram16 ram)
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
	case 0x0f: isi_cpu_wrmem(ram, pr->R[p & 0x7], v); return;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; isi_cpu_wrmem(ram, isi_cpu_rdmem(ram, pr->PC++) + pr->R[p & 0x7], v); return;
	case 0x18: isi_cpu_wrmem(ram, pr->SP, v); return;
	case 0x19: isi_cpu_wrmem(ram, pr->SP, v); return;
	case 0x1a: pr->cycl++; isi_cpu_wrmem(ram, pr->SP + isi_cpu_rdmem(ram, pr->PC++), v); return;
	case 0x1b: pr->SP = v; return;
	case 0x1c: pr->PC = v; return;
	case 0x1d: pr->EX = v; return;
	case 0x1e: pr->cycl++; isi_cpu_wrmem(ram, isi_cpu_rdmem(ram, pr->PC++), v); return;
	case 0x1f: pr->cycl++; pr->PC++; /* Fail silently */ return;
	default:
		isilog(L_DEBUG, "DCPU: Bad write: %x\n", p);
		// Should never actually happen
		return;
	}
}


// Dereference an A operand for ops
static inline uint16_t DCPU_deref(int p, struct DCPU* pr, isiram16 ram)
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
	case 0x0f: return isi_cpu_rdmem(ram, pr->R[p & 0x7]);
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; return isi_cpu_rdmem(ram, isi_cpu_rdmem(ram, pr->PC++) + pr->R[p & 0x7]);
	case 0x18: return isi_cpu_rdmem(ram, pr->SP++);
	case 0x19: return isi_cpu_rdmem(ram, pr->SP);
	case 0x1a: pr->cycl++; return isi_cpu_rdmem(ram, pr->SP + isi_cpu_rdmem(ram, pr->PC++));
	case 0x1b: return pr->SP;
	case 0x1c: return pr->PC;
	case 0x1d: return pr->EX;
	case 0x1e: pr->cycl++; return isi_cpu_rdmem(ram, isi_cpu_rdmem(ram, pr->PC++));
	case 0x1f: pr->cycl++; return isi_cpu_rdmem(ram, pr->PC++);
	default:
		return (uint16_t)(p - 0x21); /* literals */
	}
}

// Dereference a B operand for R/W ops:
static inline uint16_t DCPU_derefB(int p, struct DCPU* pr, isiram16 ram)
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
	case 0x0f: return isi_cpu_rdmem(ram, pr->R[p & 0x7]);
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: pr->cycl++; return isi_cpu_rdmem(ram, isi_cpu_rdmem(ram, pr->PC) + pr->R[p & 0x7]);
	case 0x18: return isi_cpu_rdmem(ram, --(pr->SP));
	case 0x19: return isi_cpu_rdmem(ram, pr->SP);
	case 0x1a: pr->cycl++; return isi_cpu_rdmem(ram, pr->SP + isi_cpu_rdmem(ram, pr->PC));
	case 0x1b: return pr->SP;
	case 0x1c: return pr->PC;
	case 0x1d: return pr->EX;
	case 0x1e: pr->cycl++; return isi_cpu_rdmem(ram, isi_cpu_rdmem(ram, pr->PC));
	case 0x1f: pr->cycl++; return isi_cpu_rdmem(ram, pr->PC);
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

static int DCPU_run(struct isiInfo * info, struct timespec crun)
{
	struct isiCPUInfo *l_info = (struct isiCPUInfo*)info;
	struct DCPU *pr = (struct DCPU*)info->rvstate;
	isiram16 ram = pr->memptr;
	size_t cycl = 0;
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
	if(l_info->ctl & ISICTL_STEP) {
		if(l_info->ctl & ISICTL_STEPE) {
			ccq = 1;
			l_info->ctl &= ~ISICTL_STEPE;
		} else {
			info->nrun.tv_sec = crun.tv_sec;
			info->nrun.tv_nsec = crun.tv_nsec;
			isi_addtime(&info->nrun, l_info->runrate);
		}
	} else {
		ccq = l_info->rate;
	}
	while(ccq && isi_time_lt(&info->nrun, &crun)) {

	if(pr->MODE == BURNING) {
		cycl += 3;
	}
	if(l_info->ctl & ISICTL_DEBUG) {
		if(!(l_info->ctl & ISICTL_STEP) && !(l_info->ctl & ISICTL_STEPE) && isi_cpu_isbrk(ram, pr->PC)) {
			l_info->ctl |= ISICTL_STEP | ISICTL_TRACE;
			isilog(L_DEBUG, "dcpu: break point at %04x\n", pr->PC);
			showdiag_dcpu(info, 1);
			break;
		}
		l_info->ctl &= ~ISICTL_STEPE;
	}

	if((pr->MODE & DCPUMODE_EXTINT)) {
		pr->MODE ^= DCPUMODE_EXTINT;
	} else {
		op = isi_cpu_rdmem(ram, pr->PC++);
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
				pr->msg = ISE_QINT;
				pr->dai = alu1.u;
				if(isi_message_dev(info, ISIAT_UP, &pr->msg, 10, info->nrun) == 0) {
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
				pr->msg = ISE_XINT;
				pr->dai = alu1.u;
				if(0 != (op = isi_message_dev(info, ISIAT_UP, &pr->msg, 10, info->nrun))) {
					if(op > 0) {
						//pr->wcycl = op;
						pr->MODE |= DCPUMODE_EXTINT;
						goto ecpu;
					}
				}
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
							alu2.ui = -alu2.ui;
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
	if(l_info->ctl & ISICTL_DEBUG) {
		if(l_info->ctl & ISICTL_RUNFOR) {
			if(l_info->rcycl > cycl) {
				l_info->rcycl -= cycl;
			} else {
				l_info->ctl &= ~ISICTL_RUNFOR;
				l_info->ctl |= ISICTL_STEP | ISICTL_TRACE;
				ccq = 0;
				showdiag_dcpu(info, 1);
				break;
			}
		}
	}
	if(ccq < cycl) { // used too many
		ccq = 0;
	} else {
		ccq -= cycl; // each op uses cycles
	}
	l_info->cycl += cycl;
	isi_addtime(&info->nrun, cycl * l_info->runrate);
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

