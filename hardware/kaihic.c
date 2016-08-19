
#include "../dcpuhw.h"
#include "pdi.h"

#define HIC_RBUSY       (1<<0)
#define HIC_PNOCONNECT  (1<<1)
#define HIC_RDATA       (1<<2)
#define HIC_PINVALID    (1<<3)
#define HIC_TFREE       (1<<4)
#define HIC_TIDLE       (1<<5)
#define HIC_PDATA       (1<<6)
#define HIC_RINT        (1<<14)
#define HIC_TINT        (1<<15)

/* state while this device exists/active (runtime volatile) */
struct KaiHIC_rvstate {
	uint16_t iwordrcv;
	uint16_t iwordtxe;
	uint16_t txport;
	uint16_t portcount;
	int busy;
	struct PDI_port tx; /* transmit queue */
	struct PDI_port rx[32]; /* receive buffers/queues */
};
ISIREFLECT(struct KaiHIC_rvstate,
)

/* state while running on this server (server volatile) */
struct KaiHIC_svstate {
	struct timespec tx; /* transmit timer */
	struct timespec rx[32]; /* receiver timers */
};
ISIREFLECT(struct KaiHIC_svstate,
)

static int KaiHIC_Init(struct isiInfo *info);
static int KaiHIC_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg);
struct isidcpudev KaiHIC_Meta = {0x0448,0xE0239088,MF_KAICOMM}; /* 32 port HIC / KaiComm */
struct isiConstruct KaiHIC_Con32 = {
	.objtype = 0x5000,
	.name = "kaihic32",
	.desc = "KaiComm HIC 32 Port",
	.PreInit = NULL,
	.Init = KaiHIC_Init,
	.New = KaiHIC_New,
	.rvproto = &ISIREFNAME(struct KaiHIC_rvstate),
	.svproto = &ISIREFNAME(struct KaiHIC_svstate),
	.meta = &KaiHIC_Meta
};
void KaiHIC_Register()
{
	isi_register(&KaiHIC_Con32);
}

static int KaiHIC_QueryAttach(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!dev || !info) return -1;
	if(dev->id.objtype < 0x2f00) return -1;
	return 0;
}

static int KaiHIC_Reset(struct isiInfo *info)
{
	struct KaiHIC_rvstate *dev = (struct KaiHIC_rvstate*)info->rvstate;
	int mpc = (((struct isidcpudev*)info->meta->meta)->verid & 0x0f) << 2;
	dev->portcount = mpc;
	dev->iwordrcv = 0;
	dev->iwordtxe = 0;
	dev->txport = 0;
	dev->busy = 0;
	dev->tx.stat = 0;
	for(int i = 0; i < dev->portcount; i++) {
		dev->rx[i].stat = 0;
	}
	return 0;
}

static int KaiHIC_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	return 0;
}

static int KaiHIC_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	struct KaiHIC_rvstate *dev = (struct KaiHIC_rvstate*)info->rvstate;
	struct KaiHIC_svstate *devv = (struct KaiHIC_svstate*)info->svstate;
	uint16_t bstat = 0;
	uint16_t baux = 0;
	uint16_t cport = 0;
	switch(msg[0]) {
	case 0: /* query status (C port) A=status, C=lowest */
		bstat = 0;
		cport = msg[2];
		if(cport >= dev->portcount) {
			bstat |= HIC_PNOCONNECT | HIC_PINVALID; /* port invalid */
		} else if(isi_getindex_dev(info, cport, 0)) {
			bstat |= HIC_PNOCONNECT;
		} else {
			baux = dev->rx[cport].stat;
			if(pdi_isbusy(dev->rx + cport)) bstat |= HIC_RBUSY;
			if(pdi_hasdata(dev->rx + cport)) bstat |= HIC_RDATA;
		}
		if( pdi_hasfree(&dev->tx) ) bstat |= HIC_TFREE;
		if( !pdi_isbusy(&dev->tx) ) bstat |= HIC_TIDLE;
		if(dev->iwordrcv) bstat |= HIC_RINT;
		if(dev->iwordtxe) bstat |= HIC_TINT;
		baux = 0xffff;
		for(int i = 0; i < dev->portcount; i++) {
			if(pdi_hasdata(dev->rx + i)) { baux = i; break; }
		}
		/* report status */
		msg[0] = bstat;
		msg[2] = baux;
		break;
	case 1: /* receive data (C port) B=data, C=err_status */
		cport = msg[2];
		if(cport >= dev->portcount) {
			bstat = 3; /* Fail, no data... ever. */
		} else {
			if(pdi_getword(dev->rx + cport, &baux))
				bstat = 3; /* Fail, no data */
			else if(pdi_getrxoverflow(dev->rx + cport))
				bstat = 1;
		}
		msg[1] = baux;
		msg[2] = bstat;
		break;
	case 2: /* transmit data (B data, C port) C=err_status */
		baux = msg[1];
		cport = msg[2];
		if(cport >= dev->portcount) {
			bstat = 3; /* Fail, no connection */
		} else if(isi_getindex_dev(info, cport, 0)) {
			bstat = 3; /* Fail, no connection */
		} else if(cport != dev->txport && pdi_isbusy(&dev->tx)) {
			bstat = 4;
		} else {
			dev->txport = cport;
			if(pdi_isbusy(&dev->tx)) bstat = 1;
			if(pdi_addtxword(&dev->tx, &devv->tx, baux, &mtime)) bstat = 2;
		}
		msg[2] = bstat;
		break;
	case 3: /* set interrupt (B rx_int, C tx_int) */
		dev->iwordrcv = msg[1];
		dev->iwordtxe = msg[2];
		break;
	case 4: /* name to mem (B addr, C port) mem=name, C=err_status */
		baux = msg[1];
		cport = msg[2];
		if(cport >= dev->portcount || !info->nvstate) {
			bstat = 1;
		} else if(baux > 0xfff7u || !info->mem) {
			bstat = 2;
		} else {
			int e = cport * 8 + 8;
			for(int i = cport * 8; i < e && i * 2 < info->nvsize; i++) {
				isi_hw_wrmem((isiram16)info->mem, baux, ((uint16_t*)info->nvstate)[i]);
				baux++;
			}
		}
		msg[2] = bstat;
		break;
	}
	return 0;
}

static int KaiHIC_Tick(struct isiInfo *info, struct timespec crun)
{
	struct KaiHIC_rvstate *dev = (struct KaiHIC_rvstate*)info->rvstate;
	struct KaiHIC_svstate *devv = (struct KaiHIC_svstate*)info->svstate;
	if(!isi_time_lt(&info->nrun, &crun)) return 0; /* wait for scheduled time */
	if(!info->nrun.tv_sec) {
		info->nrun.tv_sec = crun.tv_sec;
		info->nrun.tv_nsec = crun.tv_nsec;
	}
	isi_addtime(&info->nrun, 100000);
	uint16_t iom[3];
	if(pdi_process(&dev->tx, &devv->tx, &crun)) {
		iom[0] = ISE_DPSI;
		if(!pdi_getword(&dev->tx, iom+1)) {
			isi_message_dev(info, dev->txport, iom, 2, crun);
		}
		if(!pdi_isbusy(&dev->tx) && dev->iwordtxe) {
			iom[0] = ISE_XINT;
			iom[1] = 0;
			iom[2] = dev->iwordtxe;
			isi_message_dev(info, ISIAT_UP, iom, 3, crun);
		}
	}
	int isend = 0;
	for(int i = 0; i < dev->portcount; i++) {
		if(pdi_process(dev->rx + i, devv->rx + i, &crun)) {
			isend = 1;
		}
	}
	if(isend && dev->iwordrcv) {
		iom[0] = ISE_XINT;
		iom[1] = 0;
		iom[2] = dev->iwordrcv;
		isi_message_dev(info, ISIAT_UP, iom, 3, crun);
	}
	return 0;
}

static int KaiHIC_MsgIn(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	struct KaiHIC_rvstate *dev = (struct KaiHIC_rvstate*)info->rvstate;
	switch(msg[0]) { /* message type, msg[1] is device index */
	/* these should return 0 if they don't have function calls */
	case ISE_RESET: return 0; /* CPU finished reset */
	case ISE_QINT: return KaiHIC_Query(info, src, msg+2, mtime); /* HWQ executed */
	case ISE_XINT: return KaiHIC_HWI(info, src, msg+2, mtime); /* HWI executed */
	case ISE_DPSI:
		if(lsindex >= 0 && lsindex < dev->portcount) {
			struct KaiHIC_svstate *devv = (struct KaiHIC_svstate*)info->svstate;
			pdi_addrxword(dev->rx + lsindex, devv->rx + lsindex, msg[1], &mtime);
		}
		break;
	default: break;
	}
	return 1;
}

static struct isiInfoCalls KaiHIC_Calls = {
	.QueryAttach = KaiHIC_QueryAttach,
	.Reset = KaiHIC_Reset, /* power on reset */
	.MsgIn = KaiHIC_MsgIn, /* message from CPU or network */
	.RunCycles = KaiHIC_Tick, /* scheduled runtime */
};

static int KaiHIC_Init(struct isiInfo *info)
{
	info->c = &KaiHIC_Calls;
	struct KaiHIC_rvstate *dev = (struct KaiHIC_rvstate*)info->rvstate;
	int mpc = (((struct isidcpudev*)info->meta->meta)->verid & 0x0f) << 2;
	dev->portcount = mpc;
	return 0;
}

static int KaiHIC_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg)
{
	int mpc = (((struct isidcpudev*)info->meta->meta)->verid & 0x0f) << 2;
	info->nvsize = mpc * 8 * sizeof(uint16_t);
	info->nvstate = isi_alloc(info->nvsize);
	uint16_t *mpn = (uint16_t *)info->nvstate;
	for(int p = 0; p < mpc; p++) {
		mpn[8*p] = 'p';
		mpn[8*p+1] = 'd';
		mpn[8*p+2] = 'i';
		if(p < 26) {
			mpn[8*p+3] = 'a' + p;
		} else {
			mpn[8*p+3] = 'a';
			mpn[8*p+4] = 'a' + (p - 26);
		}
	}
	return 0;
}

