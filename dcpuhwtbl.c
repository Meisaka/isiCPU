/*
 * This file defines the hardware hooks table
 */
#include "dcpuhw.h"

/* Hardware hooks for HWQ and HWI */
ISIHW_DEF(Keyboard,H,T,S);
ISIHW_DEF(Nya_LEM,I,H,T,Q,S);
ISIHW_DEF(EEROM,I,R,H,Q,S);
/*
 * Feature Codes are [I]nit [R]eset [Q]uery [H]WI [T]ick [S]IZE [P]ower
 * they must be in that order below
 */
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
{0xE107,0x0001,0x30cf7406,MF_ECIV,"keyboard","Generic Keyboard",
	ISIHWT_HTS(Keyboard)},

{0xC000,0x0001,0x12d0b402,MF_ECIV,"clock","Generic Clock",
	NULL,NULL,NULL,NULL,NULL},

{0xE006,0x1802,0x7349f615,MF_NYAE,"nya_lem","Nya LEM 1802",
	ISIHWT_IQHTS(Nya_LEM)},

{0x8000,0x000b,0x4fd524c5,MF_MACK,"mack_35fd","Mackapar M35FD",
	NULL,NULL,NULL,NULL,NULL},

{0x8000,0x0003,0x42babf3c,MF_MACK,"mack_sped3","3D Display",
	NULL,NULL,NULL,NULL,NULL},

{0x8000,0x005e,0x40e41d9d,MF_NYAE,"nya_sleep","Nya Sleep Chamber",
	NULL,NULL,NULL,NULL,NULL},

{0xE003,0x0000,0x11000011,MF_ECIV,"rom","Embedded ROM",
	ISIHWT_IRQHS(EEROM)},

{0,0,0,0,NULL,0,} // End of entries
};

