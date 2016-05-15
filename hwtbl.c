/*
 * This file defines the hardware hooks table
 */
#include "dcpuhw.h"

/* Hardware functions */
void Keyboard_Register();
void Clock_Register();
void Nya_LEM_Register();
void Disk_M35FD_Register();
void EEROM_Register();

void isi_register_objects()
{
	Keyboard_Register();
	Clock_Register();
	Nya_LEM_Register();
	Disk_M35FD_Register();
	EEROM_Register();
}

