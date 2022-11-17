#ifndef ISI_NETMSG_H
#define ISI_NETMSG_H

#define ISIM_KEEPALIVE  (0x000)
#define ISIM_R_PING     (0x001)
#define ISIM_HELLO      (0x002)
#define ISIM_GOODBYE    (0x003)
#define ISIM_GET_EXTS   (0x004)
#define ISIM_GET_OPTS   (0x005)
#define ISIM_GET_EXTA   (0x006)
#define ISIM_SET_OPTS   (0x007)
#define ISIM_R_EXTS     (0x008)
#define ISIM_R_OPTS     (0x009)
#define ISIM_R_EXTA     (0x00A)
#define ISIM_DS_EXTS    (0x00D)
#define ISIM_EN_EXTS    (0x00E)

#define ISIM_PING       (0x010)
#define ISIM_GETOBJ     (0x011)
#define ISIM_SYNCALL    (0x012)
#define ISIM_GETCLASSES (0x013)
#define ISIM_GETHEIR    (0x014)

#define ISIM_NEWOBJ     (0x020)
#define ISIM_DELOBJ     (0x021)
#define ISIM_ATTACH     (0x022)
#define ISIM_DEATTACH   (0x023)
#define ISIM_START      (0x024)
#define ISIM_STOP       (0x025)
#define ISIM_ATTACHAT   (0x026)

#define ISIM_LOADOBJ    (0x02A)

#define ISIM_MSGOBJ     (0x080)
#define ISIM_MSGCHAN    (0x081)

#define ISIM_SYNCMEM16D (0x100)
#define ISIM_SYNCMEM32D (0x101)
#define ISIM_SYNCMEM16  (0x102)
#define ISIM_SYNCMEM24  (0x103)
#define ISIM_SYNCMEM32  (0x104)

#define ISIM_SYNCRVS    (0x110)
#define ISIM_SYNCSVS    (0x111)
#define ISIM_SYNCNVS    (0x112)

#define ISIM_SYNCRVSO   (0x114)
#define ISIM_SYNCSVSO   (0x115)
#define ISIM_SYNCNVSO   (0x116)

#define ISIM_REQRVS     (0x140)
#define ISIM_REQSVS     (0x141)
#define ISIM_REQNVS     (0x142)

#define ISIM_R_GETOBJ   (0x211)

#define ISIM_R_GETCLASSES (0x213)
#define ISIM_R_GETHEIR  (0x214)

#define ISIM_R_NEWOBJ   (0x220)
#define ISIM_R_DELOBJ   (0x221)
#define ISIM_R_ATTACH   (0x222)
#define ISIM_R_DEATTACH (0x223)
#define ISIM_R_START    (0x224)
#define ISIM_R_STOP     (0x225)

#define ISIM_R_LOADOBJ  (0x22A)

#define _ISIMSG(m) m
#define ISIMSG(m, f)  ((_ISIMSG(ISIM_##m) << 20) | ((f) << 13))
#endif

