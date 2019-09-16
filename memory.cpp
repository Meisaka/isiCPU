
#include "isitypes.h"

static isiClass<memory64x16> Mem6416_Con(ISIT_MEM16, "memory_16x64k", "64k x 16 Memory");

void Memory_Register()
{
	isi_register(&Mem6416_Con);
}

uint32_t isiRam::x_rd(uint32_t a) { return 0; }
void isiRam::x_wr(uint32_t a, uint32_t v) { }
uint32_t isiRam::d_rd(uint32_t a) { return 0; }
void isiRam::d_wr(uint32_t a, uint32_t v) { }
int isiRam::isbrk(uint32_t a) { return 0; }
void isiRam::togglebrk(uint32_t a) { }

uint32_t memory64x16::x_rd(uint32_t a)
{
	return this->ram[a & 0xffff];
}

int memory64x16::isbrk(uint32_t a)
{
	return (this->ctl[a & 0xffff] & ISI_RAMCTL_BREAK) ? 1:0;
}

void memory64x16::togglebrk(uint32_t a)
{
	this->ctl[a & 0xffff] ^= ISI_RAMCTL_BREAK;
}

void memory64x16::x_wr(uint32_t a, uint32_t v)
{
	a &= 0xffff;
	if(!(this->ctl[a] & ISI_RAMCTL_RDONLY)) this->ram[a] = v & 0xffff;
	if(this->ram[a] ^ ((uint16_t)this->ctl[a])) {
		//this->ctl[a] |= ISI_RAMCTL_DELTA;
		if(this->ctl[a] & ISI_RAMCTL_SYNC) this->info |= ISI_RAMINFO_SCAN;
	} else {
		//this->ctl[a] &= ~ISI_RAMCTL_DELTA;
	}
}

uint32_t memory64x16::d_rd(uint32_t a)
{
	return this->ram[a & 0xffff];
}

void memory64x16::d_wr(uint32_t a, uint32_t v)
{
	a &= 0xffff;
	if(!(this->ctl[a] & ISI_RAMCTL_RDONLY)) this->ram[a] = v & 0xffff;
	if(this->ram[a] ^ ((uint16_t)this->ctl[a])) {
		//this->ctl[a] |= ISI_RAMCTL_DELTA;
		if(this->ctl[a] & ISI_RAMCTL_SYNC) this->info |= ISI_RAMINFO_SCAN;
	} else {
		//this->ctl[a] &= ~ISI_RAMCTL_DELTA;
	}
}

