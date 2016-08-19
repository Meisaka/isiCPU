#ifndef _ISI_DEFS_H_
#define _ISI_DEFS_H_
/***
 * Constants and codes used by the isiCPU API.
 */

/* isi static classes */
#define ISIT_NONE      0
#define ISIT_SESSION   0x1000
#define ISIT_NETSYNC   0x1001
#define ISIT_PRELOAD   0x1111
#define ISIT_MEM6416   0x2001
#define ISIT_CEMEI     0x2fee /* channelized external message exchange interface */
#define ISIT_DISK      0x2fff
#define ISIT_DCPU      0x3001
/* isi Class ranges */
#define ISIT_CPU       0x3000
#define ISIT_ENDCPU    0x4000
#define ISIT_BUSDEV    0x4000
#define ISIT_ENDBUSDEV 0x5000
#define ISIT_HARDWARE  0x5000

/* isi Error codes */
#define ISIERR_SUCCESS 0
#define ISIERR_FAIL -1
#define ISIERR_NOTFOUND 1
#define ISIERR_INVALIDPARAM -2
#define ISIERR_MISSPREREQ -3
#define ISIERR_NOCOMPAT -4
#define ISIERR_NOMEM -5
#define ISIERR_FILE -6
#define ISIERR_LOADED -7
#define ISIERR_BUSY -8
#define ISIERR_NOTSUPPORTED -10
#define ISIERR_NOTALLOWED -40
#define ISIERR_ATTACHINUSE -41

/* specical attach points */
#define ISIAT_APPEND -1
#define ISIAT_INSERTSTART -2
#define ISIAT_UP -3
#define ISIAT_SESSION -4
#define ISIAT_LIMIT -4

/* isi Message Signal Types */
#define ISE_RESET 0
#define ISE_QINT 1
#define ISE_XINT 2
#define ISE_SREG 3
#define ISE_DPSI  0x10 /* parallel signaling */
#define ISE_DSSI  0x11 /* sync serial (msb first) */
#define ISE_DSSIR 0x12 /* sync serial (lsb first) */
#define ISE_DASI  0x13 /* async serial (msb first) */
#define ISE_DASIR 0x14 /* async serial (lsb first) */
#define ISE_DWIOR 0x15 /* sync serial bus /w CSMA/CA (word lsb first, lowest takes presidence) */
#define ISE_DWPKT 0x16 /* sync serial bus /w CSMA/CD (word lsb first) */
#define ISE_DISKSEEK  0x20
#define ISE_DISKWPRST 0x21
#define ISE_DISKRESET 0x22
/* 20xx internal/physical signalling */
#define ISE_CONFIG  0x2000
#define ISE_AXIS8   0x20E0
#define ISE_AXIS16  0x20E1
#define ISE_KEYDOWN 0x20E7
#define ISE_KEYUP   0x20E8

#define ISE_SUBSCRIBE 0xFFFF

#endif

