#ifndef ISI_NETMSG_H
#define ISI_NETMSG_H

#define ISIM_KEEPALIVE  (0x000)
#define ISIM_R_PING     (0x001)
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

#define ISIM_TNEWOBJ    (0x030)
#define ISIM_TLOADOBJ   (0x03A)

#define ISIM_MSGOBJ     (0x080)
#define ISIM_MSGCHAN    (0x081)
#define ISIM_SYNCMEM16  (0x0E0)
#define ISIM_SYNCMEM32  (0x0E1)
#define ISIM_SYNCRVS    (0x0E2)
#define ISIM_SYNCSVS    (0x0E3)

#define ISIM_R_GETOBJ   (0x200)
#define ISIM_L_GETOBJ   (0x201)

#define ISIM_R_GETCLASSES  (0x213)
#define ISIM_L_GETCLASSES  (0x313)
#define ISIM_R_GETHEIR  (0x214)
#define ISIM_L_GETHEIR  (0x314)

#define ISIM_R_NEWOBJ   (0x220)
#define ISIM_R_DELOBJ   (0x221)
#define ISIM_R_ATTACH   (0x222)
#define ISIM_R_START    (0x224)
#define ISIM_R_STOP     (0x225)

#define ISIM_R_TNEWOBJ  (0x230)
#define ISIM_R_TLOADOBJ (0x23A)

#define _ISIMSG(m) m
#define ISIMSG(m, f, l)  ((_ISIMSG(ISIM_##m) << 20) | ((f) << 13) | (l))
#endif

