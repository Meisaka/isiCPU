/*
 * This file defines the hardware hooks table
 */
#ifdef ISI_BUILD_DCPU
#include "dcpuhw.h"

/* Hardware functions */
void DCPU_Register();
void Memory_Register();
void Disk_Register();
void DCPUBUS_Register();
void Keyboard_Register();
void Clock_Register();
void Nya_LEM_Register();
void Disk_M35FD_Register();
void EEROM_Register();
void imva_Register();
void speaker_Register();
void vppDACA_Register();
void KaiHIC_Register();
#else
class isiInfo;
void showdiag_dcpu(const isiInfo *, int) {}
#endif
void isi_register_objects() {
#ifdef ISI_BUILD_DCPU
	Memory_Register();
	Disk_Register();
	DCPU_Register();
	DCPUBUS_Register();
	Keyboard_Register();
	Clock_Register();
	Nya_LEM_Register();
	Disk_M35FD_Register();
	EEROM_Register();
	imva_Register();
	speaker_Register();
	vppDACA_Register();
	KaiHIC_Register();
#endif
}

