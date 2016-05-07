#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>
#include "dcpu.h"
#include "cputypes.h"

#define MF_ECIV 0xeca7410c
#define MF_NYAE 0x1c6c8b36
#define MF_MACK 0x1eb37e91

struct systemhwstate {
	uint16_t msg;
};

int HWM_FreeAll(struct isiInfo *info);
int HWM_CreateBus(struct isiInfo *info);
int HWM_CreateDevice(struct isiInfo *info, const char *cfg);

typedef int (*HWMDev_SIZE)(int, const char *);
typedef int (*isi_init)(struct isiInfo *, const char *);

struct stdevtable {
	uint16_t flags;
	uint16_t verid;
	uint32_t devid;
	uint32_t mfgid;
	const char *devid_name;
	const char *devid_desc;
	isi_init InitDev;
	HWMDev_SIZE GetSize;
};

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
#define ISIHW_S(n)  int n##_SIZE(int, const char *);
#define ISIHWT_IS(n)  n##_Init, n##_SIZE

