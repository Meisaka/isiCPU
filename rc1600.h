#pragma once
#ifndef _RC1600_H_
#define _RC1600_H_

#include <stding.h>
#include <string.h>


typedef struct stRC1600 {
	uint16_t R[14];
	uint16_t RY;
	uint16_t BP;
	uint16_t SP;
	uint16_t Flags;
	uint16_t IA;
	uint16_t PC;
	uint16_t rCS;
	uint16_t rDS;
	uint16_t rSS;
	uint16_t rIS;

	unsigned long long cycl;
	
	int control;
	int cpuid;
	int hwman;
	int hwmem;
	uint8_t * memptr; // byte sized memory
	int hwcount;
	void * hwloadout;
	void * hwdata;
} RC1600;
