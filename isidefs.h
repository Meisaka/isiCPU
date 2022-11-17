#ifndef _ISI_DEFS_H_
#define _ISI_DEFS_H_
/***
 * Constants and codes used by the isiCPU API.
 */

/* isi static classes */
#define ISIT_NONE                0
#define ISIT_SESSION        0x0001 /* standard net session */
#define ISIT_SESSION_SERVER 0x0002 /* listener */
#define ISIT_SESSION_TTY    0x0003 /* interactive tty session */
#define ISIT_SESSION_REDIS  0x0004 /* Redis link */
#define ISIT_CEMEI          0x0005 /* external message interface */
#define ISIT_NETSYNC        0x0006
#define ISIT_PRELOAD        0x0007
#define ISIT_MEMORY      0x1000000
#define ISIT_MEM8        0x1000001
#define ISIT_MEM16       0x1000002
#define ISIT_MEM24       0x1000003
#define ISIT_MEM32       0x1000004
#define ISIT_MEMORY_END  0x1ffffff
#define ISIT_DISK        0x200ff00
/* isi Class ranges */
#define ISIT_CPU         0x3000000
#define ISIT_ENDCPU      0x4000000
#define ISIT_BUSDEV      0x4000000
#define ISIT_ENDBUSDEV   0x5000000
#define ISIT_HARDWARE    0x5000000

#define ISIT_IS_INFO(a) ((a) > 0x01ffffff)
#define ISIT_CLASS_MASK  0xff000000
#define ISIT_INUM_MASK  0x00ffffff

#define ISIT_IS(a,t) (((a) & ISIT_CLASS_MASK) == (t))

/* isi Error codes */
#define ISIERR_SUCCESS 0
#define ISIERR_OK 0
#define ISIERR_FAIL 1
#define ISIERR_NOTFOUND 2
#define ISIERR_INVALIDPARAM 3
#define ISIERR_MISSPREREQ 4
#define ISIERR_NOMEM 5
#define ISIERR_NOCOMPAT 6
#define ISIERR_FILE 7
#define ISIERR_LOADED 8
#define ISIERR_BUSY 9
#define ISIERR_NOTSUPPORTED 10
#define ISIERR_NOTALLOWED 40
#define ISIERR_ATTACHINUSE 41

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

