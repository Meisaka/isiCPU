#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>
#include "dcpu.h"
#include "cputypes.h"

#define HWQ_SUCCESS 1
#define HWQ_FAIL 0

#define ECIV_LO 0x410c
#define ECIV_HI 0xECA7
#define NYAE_LO 0x8b36
#define NYAE_HI 0x1c6c
#define MACK_LO 0x7e91
#define MACK_HI 0x1eb3

struct systemhwstate {
	int cpustate;
	int netwfd;
	int cpuid;
	int hwnum;
	struct timespec crun;
	uint16_t *mem;
	uint16_t *regs;
	uint16_t msg;
};

int HWM_InitLoadout(DCPU *cpu, int devct);
int HWM_FreeAll(DCPU *cpu);
int HWM_DeviceAdd(DCPU *cpu, int did);
int HWM_InitAll(DCPU *cpu);
int HWM_Query(uint16_t* reg, uint16_t hwnum, DCPU *cpu);
int HWM_HWI(uint16_t* reg, uint16_t hwnum, DCPU *cpu);

typedef int (*HWMDev_Call)(void *, struct systemhwstate*);
typedef int (*HWMDev_SIZE)();

struct stdevtable {
	uint16_t flags;
	uint16_t verid;
	uint16_t devid_hi;
	uint16_t devid_lo;
	uint16_t mfgid_hi;
	uint16_t mfgid_lo;
	char* devid_name;
	HWMDev_Call OnHWQ;
	HWMDev_Call OnHWI;
	HWMDev_Call OnTick;
	HWMDev_SIZE GetSize;
	HWMDev_Call GetPower;
};


// Hardware hooks for HWQ and HWI
int Timer_Query   (void *, struct systemhwstate *);
int Keyboard_Query(void *, struct systemhwstate *);
int Keyboard_HWI  (void *, struct systemhwstate *);
int Keyboard_Tick (void *, struct systemhwstate *);
int Keyboard_SIZE();
int Nya_LEM_Tick  (void *, struct systemhwstate *);
int Nya_LEM_Query (void *, struct systemhwstate *);
int Nya_LEM_HWI   (void *, struct systemhwstate *);
int Nya_LEM_SIZE();

