
#include "cputypes.h"

uint16_t isi_cpu_rdmem(isiram16 ram, uint16_t a)
{
	return ram->ram[a];
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

