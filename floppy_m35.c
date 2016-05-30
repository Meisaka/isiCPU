
#include "dcpuhw.h"

#define ERR_M35_NONE 0
#define ERR_M35_BUSY 1
#define ERR_M35_NO_MEDIA 2
#define ERR_M35_PROTECT 3
#define ERR_M35_EJECT 4
#define ERR_M35_SECTOR 5
#define ERR_M35_BROKEN 0xffff

/* state while this device exists/active (runtime volatile) */
struct Disk_M35FD_rvstate {
	uint16_t track;
	uint16_t sector;
	uint16_t seektrack;
	uint16_t seeksector;
	uint16_t rwaddr;
	uint16_t iword;
	uint16_t oper;
	uint16_t errcode;
};
ISIREFLECT(struct Disk_M35FD_rvstate,
	ISIR(Disk_M35FD_rvstate, uint16_t, track)
	ISIR(Disk_M35FD_rvstate, uint16_t, sector)
	ISIR(Disk_M35FD_rvstate, uint16_t, seektrack)
	ISIR(Disk_M35FD_rvstate, uint16_t, seeksector)
	ISIR(Disk_M35FD_rvstate, uint16_t, rwaddr)
	ISIR(Disk_M35FD_rvstate, uint16_t, iword)
	ISIR(Disk_M35FD_rvstate, uint16_t, oper)
	ISIR(Disk_M35FD_rvstate, uint16_t, errcode)
)

/* state while running on this server (server volatile) */
struct Disk_M35FD_svstate {
	uint16_t svbuf[512];
};
ISIREFLECT(struct Disk_M35FD_svstate,
	ISIR(Disk_M35FD_svstate, uint16_t, svbuf)
)

static int Disk_M35FD_Init(struct isiInfo *info, const uint8_t *cfg, size_t lcfg);
static struct isidcpudev Disk_M35FD_Meta = {0x000b,0x4fd524c5,MF_MACK};
static struct isiConstruct Disk_M35FD_Con = {
	ISIT_HARDWARE, "mack_35fd", "Mackapar M35FD",
	NULL, Disk_M35FD_Init, NULL,
	&ISIREFNAME(struct Disk_M35FD_rvstate), &ISIREFNAME(struct Disk_M35FD_svstate),
	&Disk_M35FD_Meta
};
void Disk_M35FD_Register()
{
	isi_register(&Disk_M35FD_Con);
}

static int Disk_M35FD_QAttach(struct isiInfo *info, struct isiInfo *dev)
{
	if(!dev || dev->id.objtype != ISIT_DISK) return -1;
	return 0;
}

static int Disk_M35FD_Reset(struct isiInfo *info)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)info->rvstate;
	dev->errcode = 0;
	dev->iword = 0;
	dev->oper = 0;
	dev->seektrack = 0;
	dev->seeksector = 0;
	dev->track = 400;
	dev->sector = 0;
	return 0;
}

static void Disk_M35FD_statechange(struct isiInfo *info, struct timespec *mtime)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)info->rvstate;
	if(dev->iword) {
		if(info->hostcpu && info->hostcpu->c->MsgIn) {
			info->hostcpu->c->MsgIn(info->hostcpu, info, &dev->iword, *mtime);
		}
	}
}

static int Disk_M35FD_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)info->rvstate;
	struct Disk_M35FD_svstate *dss = (struct Disk_M35FD_svstate*)info->svstate;
	uint16_t lerr;
	uint16_t mc;
	int i;
	int r = 0;
	switch(msg[0]) {
	case 0:
		if(info->dndev) {
			if(dev->oper) {
				msg[1] = 3;
			} else {
				msg[1] = 1; // TODO RW vs RO floppy
			}
		} else {
			msg[1] = 0;
		}
		msg[2] = dev->errcode;
		if(dev->errcode) Disk_M35FD_statechange(info, &crun);
		dev->errcode = ERR_M35_NONE;
		break;
	case 1:
		dev->iword = msg[3];
		break;
	case 2:
		msg[1] = 0;
		lerr = dev->errcode;
		if(dev->oper != 0) dev->errcode = ERR_M35_BUSY;
		else if(!info->dndev) dev->errcode = ERR_M35_NO_MEDIA;
		else if(msg[3] >= 1440) dev->errcode = ERR_M35_SECTOR;
		else {
			dev->seeksector = msg[3];
			dev->seektrack = dev->seeksector / 18;
			dev->rwaddr = msg[4];
			dev->oper = 1;
			msg[1] = 1;
			Disk_M35FD_statechange(info, &crun);
		}
		if(lerr != dev->errcode) Disk_M35FD_statechange(info, &crun);
		break;
	case 3:
		msg[1] = 0;
		lerr = dev->errcode;
		if(dev->oper != 0) dev->errcode = ERR_M35_BUSY;
		else if(!info->dndev) dev->errcode = ERR_M35_NO_MEDIA;
		else if(msg[3] >= 1440) dev->errcode = ERR_M35_SECTOR;
		else if(0) dev->errcode = ERR_M35_PROTECT; /* TODO write_protect */
		else {
			dev->seeksector = msg[3];
			dev->seektrack = dev->seeksector / 18;
			mc = dev->rwaddr = msg[4];
			for(i = 0; i < 512; i++) {
				dss->svbuf[i] = isi_hw_rdmem((isiram16)info->mem, mc);
				mc++;
			}
			dev->oper = 2;
			r = 512;
			msg[1] = 1;
			Disk_M35FD_statechange(info, &crun);
		}
		if(lerr != dev->errcode) Disk_M35FD_statechange(info, &crun);
		break;
	}
	return r;
}

static int Disk_M35FD_Tick(struct isiInfo *info, struct timespec crun)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)info->rvstate;
	struct Disk_M35FD_svstate *dss = (struct Disk_M35FD_svstate*)info->svstate;
	while(isi_time_lt(&info->nrun, &crun)) {
		if(dev->oper) {
			if(!info->dndev || !info->dndev->c->MsgIn) {
				dev->errcode = ERR_M35_EJECT;
				dev->oper = 0;
				Disk_M35FD_statechange(info, &info->nrun);
				continue;
			}
			if(dev->seektrack != dev->track) {
				isi_addtime(&info->nrun, 2400000);
				dev->track += (dev->seektrack < dev->track) ?-1:1;
				dev->sector += 73;
				dev->sector %= 18;
			} else {
				dev->sector++;
				dev->sector %= 18;
				isi_addtime(&info->nrun, 32573);
				if((dev->seeksector + 1) % 18 == dev->sector) {
					/* read success */
					struct isiDiskSeekMsg dseek;
					uint16_t *dr;
					isi_disk_getblock(info->dndev, (void**)&dr);
					dr += (512 * (dev->seeksector & 3));
					dseek.mcode = 0x0020;
					dseek.block = dev->seeksector >> 2;
					info->dndev->c->MsgIn(info->dndev, info, (uint16_t*)&dseek, crun);
					if(dev->oper == 1) {
						uint16_t mc;
						mc = dev->rwaddr;
						for(int i = 0; i < 512; i++) {
							isi_hw_wrmem((isiram16)info->mem, mc, dr[i]);
							mc++;
						}
					} else if(dev->oper == 2) {
						for(int i = 0; i < 512; i++) {
							dr[i] = dss->svbuf[i];
						}
					}
					dev->oper = 0;
					Disk_M35FD_statechange(info, &info->nrun);
				}
			}
		} else {
			if(!info->nrun.tv_sec) {
				info->nrun.tv_sec = crun.tv_sec;
				info->nrun.tv_nsec = crun.tv_nsec;
			}
			isi_addtime(&info->nrun, 50000000);
			dev->sector += 1535;
			dev->sector %= 18;
		}
	}
	return 0;
}

static int Disk_M35FD_MsgIn(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case 0: return 0; /* CPU finished reset */
	case 1: return 0; /* HWQ executed */
	case 2: return Disk_M35FD_HWI(info, src, msg+2, mtime); /* HWI executed */
	default: break;
	}
	return 1;
}

static struct isiInfoCalls Disk_M35FDCalls = {
	.Reset = Disk_M35FD_Reset, /* power on reset */
	.MsgIn = Disk_M35FD_MsgIn, /* message from CPU or network */
	.RunCycles = Disk_M35FD_Tick, /* scheduled runtime */
	.QueryAttach = Disk_M35FD_QAttach
};

static int Disk_M35FD_Init(struct isiInfo *info, const uint8_t *cfg, size_t lcfg)
{
	info->c = &Disk_M35FDCalls;
	return 0;
}

