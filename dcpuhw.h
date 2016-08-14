#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>
#include "cputypes.h"

#define MF_ECIV 0xeca7410c
#define MF_NYAE 0x1c6c8b36
#define MF_MACK 0x1eb37e91
#define MF_MEI 0x59ea5742
#define MF_KAICOMM 0xa87c900e
#define MF_OTEC 0xb8badde8
#define MF_RINYU 0xc2200311

struct isidcpudev {
	uint16_t verid;
	uint32_t devid;
	uint32_t mfgid;
};

