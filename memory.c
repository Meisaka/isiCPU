
#include "cputypes.h"

static struct isiConstruct Mem6416_Con = {
	ISIT_MEM6416, "memory_16x64k", "64k x 16 Memory",
	0, 0, 0,
	NULL, NULL,
	0
};

void Memory_Register()
{
	isi_register(&Mem6416_Con);
}

uint16_t isi_cpu_rdmem(isiram16 ram, uint16_t a)
{
	return ram->ram[a];
}

int isi_cpu_isbrk(isiram16 ram, uint16_t a)
{
	return (ram->ctl[a] & ISI_RAMCTL_BREAK) ? 1:0;
}

void isi_cpu_togglebrk(isiram16 ram, uint16_t a)
{
	ram->ctl[a] ^= ISI_RAMCTL_BREAK;
}

void isi_cpu_wrmem(isiram16 ram, uint16_t a, uint16_t v)
{
	if(!(ram->ctl[a] & ISI_RAMCTL_RDONLY)) ram->ram[a] = v;
	if(ram->ram[a] ^ ((uint16_t)ram->ctl[a])) {
		//ram->ctl[a] |= ISI_RAMCTL_DELTA;
		if(ram->ctl[a] & ISI_RAMCTL_SYNC) ram->info |= ISI_RAMINFO_SCAN;
	} else {
		//ram->ctl[a] &= ~ISI_RAMCTL_DELTA;
	}
}

uint16_t isi_hw_rdmem(isiram16 ram, uint16_t a)
{
	return ram->ram[a];
}

void isi_hw_wrmem(isiram16 ram, uint16_t a, uint16_t v)
{
	if(!(ram->ctl[a] & ISI_RAMCTL_RDONLY)) ram->ram[a] = v;
	if(ram->ram[a] ^ ((uint16_t)ram->ctl[a])) {
		//ram->ctl[a] |= ISI_RAMCTL_DELTA;
		if(ram->ctl[a] & ISI_RAMCTL_SYNC) ram->info |= ISI_RAMINFO_SCAN;
	} else {
		//ram->ctl[a] &= ~ISI_RAMCTL_DELTA;
	}
}

