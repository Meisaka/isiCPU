#!/bin/sh

NAME=$1
cat <<ENDOFLINE

#include "../dcpuhw.h"

/* state while this device exists/active (runtime volatile) */
struct ${NAME}_rvstate {
};
ISIREFLECT(struct ${NAME}_rvstate,
)

/* state while running on this server (server volatile) */
struct ${NAME}_svstate {
};
ISIREFLECT(struct ${NAME}_svstate,
)
/* SIZE call for variable size structs */
int ${NAME}_SIZE(int t, size_t *sz)
{
	if(t == 0) return *sz = sizeof(struct ${NAME}_rvstate);
	if(t == 1) return sizeof(struct ${NAME}_svstate);
	return 0;
}
static int ${NAME}_Init(struct isiInfo *info);
/* static int ${NAME}_PreInit(struct isiInfo *info); */
static int ${NAME}_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg);
struct isidcpudev ${NAME}_Meta = {0x0000,0x00000000,MF_ECIV};
struct isiConstruct ${NAME}_Con = {
	.objtype = 0x5000,
	.name = "${NAME}",
	.desc = "Default_Template_for_${NAME}",
	.PreInit = NULL, /* ${NAME}_PreInit, */
	.Init = ${NAME}_Init,
	.New = ${NAME}_New,
	.QuerySize = ${NAME}_SIZE,
	.rvproto = &ISIREFNAME(struct ${NAME}_rvstate),
	.svproto = &ISIREFNAME(struct ${NAME}_svstate),
	.meta = &${NAME}_Meta
};
void ${NAME}_Register()
{
	isi_register(&${NAME}_Con);
}

static int ${NAME}_Reset(struct isiInfo *info)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	return 0;
}

static int ${NAME}_OnReset(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, isi_time_t mtime)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	return 0;
}

static int ${NAME}_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, isi_time_t mtime)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	return 0;
}

static int ${NAME}_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, isi_time_t crun)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	switch(msg[0]) {
	case 0:
		break;
	}
	return 0;
}

static int ${NAME}_Tick(struct isiInfo *info, isi_time_t crun)
{
	struct ${NAME}_rvstate *dev = (struct ${NAME}_rvstate*)info->rvstate;
	// if(!isi_time_lt(&info->nrun, &crun)) return 0; /* wait for scheduled time */
	return 0;
}

static int ${NAME}_MsgIn(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, isi_time_t mtime);
{
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case ISE_RESET: return ${NAME}_OnReset(info, src, msg+2, mtime); /* CPU finished reset */
	case ISE_QINT: return ${NAME}_Query(info, src, msg+2, mtime); /* HWQ executed */
	case ISE_XINT: return ${NAME}_HWI(info, src, msg+2, mtime); /* HWI executed */
	default: break;
	}
	return 1;
}

static struct isiInfoCalls ${NAME}_Calls = {
	.Reset = ${NAME}_Reset, /* power on reset */
	.MsgIn = ${NAME}_MsgIn, /* message from CPU or network */
	.RunCycles = ${NAME}_Tick, /* scheduled runtime */
};

static int ${NAME}_Init(struct isiInfo *info)
{
	info->c = &${NAME}_Calls;
	return 0;
}

static int ${NAME}_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg)
{
	return 0;
}

ENDOFLINE

