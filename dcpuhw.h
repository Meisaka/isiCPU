#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>
#include "dcpu.h"
#include "cputypes.h"

#define HWQ_SUCCESS 1
#define HWQ_FAIL 0

#define MF_ECIV 0xeca7410c
#define MF_NYAE 0x1c6c8b36
#define MF_MACK 0x1eb37e91

struct systemhwstate {
	struct isiSession * net;
	isiram16 mem;
	uint16_t msg;
};

int HWM_FreeAll(struct isiInfo *info);
int HWM_CreateBus(struct isiInfo *info);
int HWM_CreateDevice(struct isiInfo *info, const char *cfg);

typedef int (*HWMDev_Call)(struct isiInfo *, struct systemhwstate *, struct timespec);
typedef int (*HWMDev_SIZE)(int, const char *);
typedef int (*isi_init)(struct isiInfo *, const char *);
typedef int (*HWMDev_Reset)(struct isiInfo *, struct isiInfo *, struct timespec);

struct stdevtable {
	uint16_t flags;
	uint16_t verid;
	uint32_t devid;
	uint32_t mfgid;
	const char *devid_name;
	const char *devid_desc;
	isi_init InitDev;
	HWMDev_Reset OnReset;
	isi_message OnHWQ;
	isi_message OnHWI;
	HWMDev_Call OnTick;
	HWMDev_SIZE GetSize;
	HWMDev_Call GetPower;
};

#define ISIDEVCALL struct isiInfo *, struct systemhwstate *, struct timespec
#define ISIDEVMSG struct isiInfo *, struct isiInfo *, uint16_t *, struct timespec
#define ISIHW_DEF1(n, x0) ISIHW_##x0(n)
#define ISIHW_DEF2(n, x0, x1...) ISIHW_DEF1(n, x1) ISIHW_##x0(n)
#define ISIHW_DEF3(n, x0, x1...) ISIHW_DEF2(n, x1) ISIHW_##x0(n)
#define ISIHW_DEF4(n, x0, x1...) ISIHW_DEF3(n, x1) ISIHW_##x0(n)
#define ISIHW_DEF5(n, x0, x1...) ISIHW_DEF4(n, x1) ISIHW_##x0(n)
#define ISIHW_DEF6(n, x0, x1...) ISIHW_DEF5(n, x1) ISIHW_##x0(n)
#define ISIHW_DEF7(n, x0, x1...) ISIHW_DEF6(n, x1) ISIHW_##x0(n)
#define ISIHW_DEF8(n, x0, x1...) ISIHW_DEF7(n, x1) ISIHW_##x0(n)
#define _ISI_NUM_ARGS2(X,X8,X7,X6,X5,X4,X3,X2,X1,N,...) N
#define _ISI_NUM_ARGS(ia...) _ISI_NUM_ARGS2(0,ia,8,7,6,5,4,3,2,1,0)
#define _ISI_HWDEF(n, IA, ia...) ISIHW_DEF ## IA(n, ia)
#define _ISI_2HWDEF(n, IA, ia...) _ISI_HWDEF(n, IA, ia)
#define ISIHW_DEF(n, ia...) _ISI_2HWDEF(n, _ISI_NUM_ARGS(ia), ia)

#define ISIHW_I(n)  int n##_Init (struct isiInfo *, const char *);
#define ISIHW_R(n)  int n##_Reset(struct isiInfo *, struct isiInfo *, struct timespec);
#define ISIHW_H(n)  int n##_HWI  (ISIDEVMSG);
#define ISIHW_T(n)  int n##_Tick (ISIDEVCALL);
#define ISIHW_Q(n)  int n##_Query(ISIDEVMSG);
#define ISIHW_P(n)  int n##_Power();
#define ISIHW_S(n)  int n##_SIZE(int, const char *);
#define ISIHWT_QHTSP(n) NULL, NULL, n##_Query, n##_HWI, n##_Tick, n##_SIZE, n##_Power
#define ISIHWT_QHTS(n)  NULL, NULL, n##_Query, n##_HWI, n##_Tick, n##_SIZE, NULL
#define ISIHWT_IQHTS(n) n##_Init, NULL, n##_Query, n##_HWI, n##_Tick, n##_SIZE, NULL
#define ISIHWT_HTSP(n)  NULL, NULL, NULL, n##_HWI, n##_Tick, n##_SIZE, n##_Power
#define ISIHWT_HTS(n)   NULL, NULL, NULL, n##_HWI, n##_Tick, n##_SIZE, NULL
#define ISIHWT_IHTS(n)  n##_Init, NULL, NULL, n##_HWI, n##_Tick, n##_SIZE, NULL
#define ISIHWT_HS(n)    NULL, NULL, NULL, n##_HWI, NULL, n##_SIZE, NULL
#define ISIHWT_QHS(n)   NULL, NULL, n##_Query, n##_HWI, NULL, n##_SIZE, NULL
#define ISIHWT_IHS(n)   n##_Init, NULL, NULL, n##_HWI, NULL, n##_SIZE, NULL
#define ISIHWT_IQHS(n)  n##_Init, NULL, n##_Query, n##_HWI, NULL, n##_SIZE, NULL
#define ISIHWT_IRQHS(n) n##_Init, n##_Reset, n##_Query, n##_HWI, NULL, n##_SIZE, NULL

