
#include "../dcpuhw.h"

#define ERR_M35_NONE 0
#define ERR_M35_BUSY 1
#define ERR_M35_NO_MEDIA 2
#define ERR_M35_PROTECT 3
#define ERR_M35_EJECT 4
#define ERR_M35_SECTOR 5
#define ERR_M35_BROKEN 0xffff

#define ST_M35_NO_MEDIA 0
#define ST_M35_IDLE 1
#define ST_M35_IDLEP 2
#define ST_M35_BUSY 3
#define ST_M35_BUSYREAD 3
#define ST_M35_BUSYWRITE 4

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

static int Disk_M35FD_Init(struct isiInfo *info);
static struct isidcpudev Disk_M35FD_Meta = {0x000b,0x4fd524c5,MF_MACK};
static struct isiConstruct Disk_M35FD_Con = {
	.objtype = ISIT_HARDWARE,
	.name = "mack_35fd",
	.desc = "Mackapar M35FD",
	.Init = Disk_M35FD_Init,
	.rvproto = &ISIREFNAME(struct Disk_M35FD_rvstate),
	.svproto = &ISIREFNAME(struct Disk_M35FD_svstate),
	.meta = &Disk_M35FD_Meta
};
void Disk_M35FD_Register()
{
	isi_register(&Disk_M35FD_Con);
}

static int Disk_M35FD_QAttach(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!dev || (point != ISIAT_UP && dev->id.objtype != ISIT_DISK)) return -1;
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
	uint16_t iom[3];
	if(dev->iword) {
		iom[0] = 2;
		iom[1] = 0;
		iom[2] = dev->iword;
		isi_message_dev(info, ISIAT_UP, iom, 3, *mtime);
	}
}

static int Disk_M35FD_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, int len, struct timespec crun)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)info->rvstate;
	struct Disk_M35FD_svstate *dss = (struct Disk_M35FD_svstate*)info->svstate;
	uint16_t lerr;
	uint16_t mc;
	int i;
	int r = 0;
	struct isiInfo *media = 0;
	isi_getindex_dev(info, 0, &media);
	switch(msg[0]) {
	case 0:
		if(media) {
			if(dev->oper >= ST_M35_BUSY) {
				msg[1] = ST_M35_BUSY;
			} else {
				msg[1] = dev->oper;
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
		if(dev->oper >= ST_M35_BUSY) dev->errcode = ERR_M35_BUSY;
		else if(!media) dev->errcode = ERR_M35_NO_MEDIA;
		else if(msg[3] >= 1440) dev->errcode = ERR_M35_SECTOR;
		else {
			dev->seeksector = msg[3];
			dev->seektrack = dev->seeksector / 18;
			struct isiDiskSeekMsg dseek;
			dseek.mcode = 0x0020;
			dseek.block = dev->seeksector >> 2;
			isi_message_dev(info, 0, (uint16_t*)&dseek, 4, crun);
			dev->rwaddr = msg[4];
			dev->oper = ST_M35_BUSYREAD;
			msg[1] = 1;
			Disk_M35FD_statechange(info, &crun);
		}
		if(lerr != dev->errcode) Disk_M35FD_statechange(info, &crun);
		break;
	case 3:
		msg[1] = 0;
		lerr = dev->errcode;
		if(dev->oper >= ST_M35_BUSY) dev->errcode = ERR_M35_BUSY;
		else if(!media) dev->errcode = ERR_M35_NO_MEDIA;
		else if(msg[3] >= 1440) dev->errcode = ERR_M35_SECTOR;
		else if(isi_disk_isreadonly(media)) dev->errcode = ERR_M35_PROTECT; /* TODO write_protect */
		else {
			dev->seeksector = msg[3];
			dev->seektrack = dev->seeksector / 18;
			struct isiDiskSeekMsg dseek;
			dseek.mcode = 0x0020;
			dseek.block = dev->seeksector >> 2;
			isi_message_dev(info, 0, (uint16_t*)&dseek, 4, crun);
			mc = dev->rwaddr = msg[4];
			for(i = 0; i < 512; i++) {
				dss->svbuf[i] = isi_hw_rdmem((isiram16)info->mem, mc);
				mc++;
			}
			dev->oper = ST_M35_BUSYWRITE;
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
	struct isiInfo *media = 0;
	if(!info->nrun.tv_sec) {
		info->nrun.tv_sec = crun.tv_sec;
		info->nrun.tv_nsec = crun.tv_nsec;
	}
	while(isi_time_lt(&info->nrun, &crun)) {
		if(dev->oper >= ST_M35_BUSY) {
			if(isi_getindex_dev(info, 0, &media) || !media->c->MsgIn) {
				dev->errcode = ERR_M35_EJECT;
				dev->oper = ST_M35_NO_MEDIA;
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
				uint32_t seekblock;
				isi_disk_getindex(media, &seekblock);
				if((dev->seeksector + 1) % 18 == dev->sector && seekblock == dev->seeksector >> 2) {
					/* read success */
					uint16_t *dr;
					isi_disk_getblock(media, (void**)&dr);
					dr += (512 * (dev->seeksector & 3));
					if(dev->oper == ST_M35_BUSYREAD) {
						uint16_t mc;
						mc = dev->rwaddr;
						for(int i = 0; i < 512; i++) {
							isi_hw_wrmem((isiram16)info->mem, mc, dr[i]);
							mc++;
						}
					} else if(dev->oper == ST_M35_BUSYWRITE) {
						for(int i = 0; i < 512; i++) {
							dr[i] = dss->svbuf[i];
						}
					}
					dev->oper = isi_disk_isreadonly(media)? ST_M35_IDLEP:ST_M35_IDLE;
					Disk_M35FD_statechange(info, &info->nrun);
				}
			}
		} else {
			if(isi_getindex_dev(info, 0, &media)) {
				if(dev->oper != ST_M35_NO_MEDIA) {
					dev->oper = ST_M35_NO_MEDIA;
					Disk_M35FD_statechange(info, &info->nrun);
				}
			} else {
				if(dev->oper == ST_M35_NO_MEDIA) {
					dev->oper = isi_disk_isreadonly(media)? ST_M35_IDLEP:ST_M35_IDLE;
					Disk_M35FD_statechange(info, &info->nrun);
				}
			}
			isi_addtime(&info->nrun, 50000000);
			dev->sector += 1535;
			dev->sector %= 18;
		}
	}
	return 0;
}

static int Disk_M35FD_MsgIn(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case 0: return 0; /* CPU finished reset */
	case 1: return 0; /* HWQ executed */
	case 2:
		if(len < 10) return -1;
		return Disk_M35FD_HWI(info, src, msg+2, len - 2, mtime); /* HWI executed */
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

static int Disk_M35FD_Init(struct isiInfo *info)
{
	info->c = &Disk_M35FDCalls;
	return 0;
}

