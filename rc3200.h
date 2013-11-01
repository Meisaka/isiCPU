#pragma once
#ifndef _RC3200_H_
#define _RC3200_H_

#include <stding.h>
#include <string.h>


typedef struct stRC3200 {
	uint32_t R[32];
	uint32_t RY;
	uint32_t Flags;
	uint32_t IA;
	uint32_t PC;

	unsigned long long cycl;
	
	int control;
	int cpuid;
	int hwman;
	int hwmem;
	uint8_t * memptr; // byte sized memory
	int hwcount;
	void * hwloadout;
	void * hwdata;
} RC3200;

// Setup, allocate, and reset
void RC3200_Init(RC3200 *);
// Insert or replace a virtual slot with memory
void RC3200_InsertROM(RC3200 *, uint32_t vaddr, uint8_t *mem, uint32_t sz);
void RC3200_InsertRAM(RC3200 *, uint32_t vaddr, uint8_t *mem, uint32_t sz);
// Remove virtual slot memory
void RC3200_RemoveMem(RC3200 *, uint32_t vaddr);
// send reset
void RC3200_Reset(RC3200 *);

// Run instruction
int RC3200_run(RC3200 *);
// Send interupt
int RC3200_Interupt(RC3200 *, uint32_t msg);
// Can interupt
int RC3200_GetInteruptStatus();
