#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>
#include "cputypes.h"

#define MF_ECIV 0xeca7410c
#define MF_NYAE 0x1c6c8b36
#define MF_MACK 0x1eb37e91

struct isidcpudev {
	uint16_t verid;
	uint32_t devid;
	uint32_t mfgid;
};

