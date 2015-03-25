/*
 * This file defines the hardware hooks table
 */
#include "dcpuhw.h"

/* Flags:
 * 0x8000 - Generic Query -- Used for devices not fully implemented, or the lazy
 * 0x4000 - Valid device -- Indicates device can be assigned to DCPU
 * 0x2000 - Requires storage and includes size function
 * 0x0001 - Handles HWQ
 * 0x0002 - Handles HWI
 * 0x0004 - Handles Tick
 * 0x0008 - Handles Energy Tick
 * == 0x0000 - End of list
 */
struct stdevtable devtable[] = {
{0x6107,0x0001,0x30cf,0x7406,ECIV_HI,ECIV_LO,"Keyboard",
	Keyboard_Query,Keyboard_HWI,Keyboard_Tick,Keyboard_SIZE,NULL},

{0x4000,0x0001,0x12d0,0xb402,ECIV_HI,ECIV_LO,"Clock",
	Timer_Query,NULL,NULL,NULL,NULL},

{0xE006,0x1802,0x7349,0xf615,NYAE_HI,NYAE_LO,"Nya LEM",
	Nya_LEM_Query,Nya_LEM_HWI,Nya_LEM_Tick,Nya_LEM_SIZE,NULL},

{0x8000,0x000b,0x4fd5,0x24c5,MACK_HI,MACK_LO,"Mackapar M35FD",
	NULL,NULL,NULL,NULL,NULL}, // M35FD

{0x8000,0x0003,0x42ba,0xbf3c,MACK_HI,MACK_LO,"3D Display",
	NULL,NULL,NULL,NULL,NULL}, // 3D Vector Display

{0x8000,0x005e,0x40e4,0x1d9d,NYAE_HI,NYAE_LO,"Nya Sleep Chamber",
	NULL,NULL,NULL,NULL,NULL}, // Sleep Chamber

{0,0,0,0,0,0,NULL,
	0,0,0,0,0} // End of entries
};

