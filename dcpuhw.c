
#include "dcpuhw.h"
#define DEBUG_DCPUHW 0
extern struct stdevtable devtable[];

int Timer_Query(void *hwd,struct systemhwstate * isi)
{
	isi->regs[0] = 0xb402;
	isi->regs[1] = 0x12d0;
	isi->regs[2] = 1;
	isi->regs[3] = ECIV_LO;
	isi->regs[4] = ECIV_HI;
	return HWQ_SUCCESS;
}

struct HWM_loadout {
	int id;
	int rofs;
};

int HWM_GetSize(int idt)
{
#if DEBUG_MEM == 1
	fprintf(stderr,"[%s] GetSize()\n", devtable[idt].devid_name);
#endif
	if((devtable[idt].flags & 0x6000) == 0x6000) {
		return devtable[idt].GetSize();
	} else {
		return 0;
	}
}

int HWM_InitLoadout(DCPU *cpu, int devct)
{
	void * blk;
	int i;
	if(!cpu) return -1;
	i = sizeof(struct HWM_loadout) * devct;
#if DEBUG_MEM == 1
	fprintf(stderr, "hwm: init loadout (+%i)\n",i);
#endif
	blk = malloc(i);
	if(!blk) {
		perror("malloc");
		return -1;
	}
	memset(blk, 0, i);
	cpu->hwloadout = blk;
	cpu->hwcount = devct;
	cpu->hwman = 0;
	cpu->hwmem = 0;
	return 0;
}

int HWM_FreeAll(DCPU *cpu)
{
	if(!cpu) return -1;
	if(cpu->hwdata) {
#if DEBUG_MEM == 1
		fprintf(stderr, "hwm: free HW mem\n");
#endif
		free(cpu->hwdata);
	}
	if(cpu->hwloadout) {
#if DEBUG_MEM == 1
		fprintf(stderr, "hwm: free HW-config\n");
#endif
		free(cpu->hwloadout);
	}
	cpu->hwdata = NULL;
	cpu->hwloadout = NULL;
	return 0;
}

int HWM_DeviceAdd(DCPU *cpu, int did)
{
	int i,k;
	struct HWM_loadout *hws;
	hws = (struct HWM_loadout*)cpu->hwloadout;
	k = cpu->hwman;
	if(k < cpu->hwcount) {
		i = HWM_GetSize(did);
#if DEBUG_MEM == 1
		fprintf(stderr, "hwinit: adding %i size %i)\n", did, i);
#endif
		hws[k].id = did;
		hws[k].rofs = cpu->hwmem;
		cpu->hwmem += i;
		cpu->hwman++;
		return 0;
	} else {
		return -5;
	}
}

int HWM_InitAll(DCPU *cpu)
{
	void * blk;
	int i;
	blk = malloc(i = cpu->hwmem);
	if(!blk) { perror("malloc"); return -5; }
	cpu->hwdata = blk;
	memset(blk, 0, cpu->hwmem);
#if DEBUG_MEM == 1
	fprintf(stderr, "hwinit: malloc %i\n", i);
#endif
	return 0;
}

int HWM_Query(uint16_t* reg, uint16_t hwnum, DCPU *cpu)
{
	struct systemhwstate lhwi;
	lhwi.cpustate = 0;
	lhwi.netwfd = 0;
	lhwi.cpuid = cpu->cpuid;
	lhwi.hwnum = hwnum;
	lhwi.mem = cpu->memptr;
	lhwi.regs = cpu->R;
	lhwi.msg = 0;

	int i, knt, r;
	struct HWM_loadout *hws;
	char * hwhw;

	hws = (struct HWM_loadout*)cpu->hwloadout;
	hwhw= (char*)cpu->hwdata;
	knt = cpu->hwcount;
	// find hardware
	if(hwnum < cpu->hwcount) {
		knt = hws[hwnum].id;
		i = hws[hwnum].rofs;
	} else {
		return 0;
	}
#if DEBUG_DCPUHW == 1
	fprintf(stderr, "hwmhwq: %s %i %04x ", devtable[knt].devid_name, i, devtable[knt].flags);
#endif
	// call it in context
	if((devtable[knt].flags & 0x4001) == 0x4001) {
		r = devtable[knt].OnHWQ(&hwhw[i], &lhwi);
	} else if(devtable[knt].flags & 0x8000) {
		cpu->R[0] = devtable[knt].devid_lo;
		cpu->R[1] = devtable[knt].devid_hi;
		cpu->R[2] = devtable[knt].verid;
		cpu->R[3] = devtable[knt].mfgid_lo;
		cpu->R[4] = devtable[knt].mfgid_hi;
		r = HWQ_SUCCESS;
	}
#if DEBUG_DCPUHW == 1
	fprintf(stderr, ": %04x %04x %04x %04x %04x \n", cpu->R[0], cpu->R[1], cpu->R[2], cpu->R[3], cpu->R[4]);
#endif
	return r;
}

int HWM_HWI(uint16_t* reg, uint16_t hwnum, DCPU *cpu)
{
	struct systemhwstate lhwi;
	lhwi.cpustate = 0;
	lhwi.netwfd = 0;
	lhwi.cpuid = cpu->cpuid;
	lhwi.hwnum = hwnum;
	lhwi.mem = cpu->memptr;
	lhwi.regs = reg;
	lhwi.msg = 0;

	int i, knt, r;
	struct HWM_loadout *hws;
	char * hwhw;

	hws = (struct HWM_loadout*)cpu->hwloadout;
	hwhw= (char*)cpu->hwdata;
	knt = cpu->hwcount;
	// find hardware
	if(hwnum < cpu->hwcount) {
		knt = hws[hwnum].id;
		i = hws[hwnum].rofs;
	} else {
		return 0;
	}
	// call it in context
	if((devtable[knt].flags & 0x4002) == 0x4002) {
#if DEBUG_DCPUHW == 1
		if(!(devtable[knt].flags & 0x0100))
			fprintf(stderr, "[HWIA %s %04x]\n", devtable[knt].devid_name, devtable[knt].flags);
#endif
		r = devtable[knt].OnHWI(&hwhw[i], &lhwi);
	} else {
#if DEBUG_DCPUHW == 1
	fprintf(stderr, "[HWIN %s %04x]\n", devtable[knt].devid_name, devtable[knt].flags);
#endif
	       r = 0;
	}
	return r;
}

int HWM_TickAll(DCPU *cpu, struct timespec crun, int fdnet, int msgin)
{
	int i,k, f;
	struct HWM_loadout *hws;
	char * hwhw;
	struct systemhwstate lhwi;
	lhwi.cpustate = 0;
	lhwi.netwfd = fdnet;
	lhwi.cpuid = cpu->cpuid;
	lhwi.mem = cpu->memptr;
	lhwi.regs = cpu->R;
	lhwi.msg = 0;
	lhwi.crun.tv_sec = crun.tv_sec;
	lhwi.crun.tv_nsec = crun.tv_nsec;
	hws = (struct HWM_loadout*)cpu->hwloadout;
	hwhw= (char*)cpu->hwdata;
	for(k = 0; k < cpu->hwcount; k++) {
		i = hws[k].id;
		f = hws[k].rofs;
		lhwi.hwnum = k;
		// Switched to a dynamic device table
		lhwi.msg = msgin;
		if((devtable[i].flags & 0x4004) == 0x4004) {
			if(devtable[i].OnTick(&hwhw[f],&lhwi)) {
				DCPU_interupt(cpu, lhwi.msg);
			}
		}
	}
	return 0;
}

