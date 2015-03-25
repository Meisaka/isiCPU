
#include "rc3200.h"

// Setup, allocate, and reset
void RC3200_Init(RC3200 * cpu)
{
	RC3200_Reset(cpu);
}

// Insert or replace a virtual slot with memory
void RC3200_InsertROM(RC3200 * cpu, uint32_t vaddr, uint8_t *mem, uint32_t sz)
{
}

void RC3200_InsertRAM(RC3200 * cpu, uint32_t vaddr, uint8_t *mem, uint32_t sz)
{
}

// Remove virtual slot memory
void RC3200_RemoveMem(RC3200 * cpu, uint32_t vaddr)
{
}

// send reset
void RC3200_Reset(RC3200 * cpu)
{
	int i;
	for(i=32; i; cpu->R[--i] = 0); // clear registers
	cpu->RY = 0;
	cpu->PC = 0;
	cpu->IA = 0;
	cpu->Flags = 0;
	cpu->cycl = 0;
}

// Run instruction
int RC3200_run(RC3200 * cpu)
{
}

// Send interupt
int RC3200_Interupt(RC3200 * cpu, uint32_t msg)
{
}

// Can interupt
int RC3200_GetInteruptStatus()
{
}
