#include "pdi.h"

int pdi_addrxword(struct PDI_port *p, struct timespec *pt, uint16_t word, struct timespec const *now)
{
	int dc = PDI_GETC(p->stat);
	if(dc < PDI_BUFLIMIT) {
		p->dat[dc++] = word;
	} else {
		p->stat |= PDI_OVFLAG;
		return -1;
	}
	p->stat = (p->stat & ~(PDI_MASKC)) | PDI_IBUSY | PDI_SETC(dc);
	return 0;
}

int pdi_addtxword(struct PDI_port *p, struct timespec *pt, uint16_t word, struct timespec const *now)
{
	int dc = PDI_GETC(p->stat);
	if(dc < PDI_BUFLIMIT) {
		p->dat[dc++] = word;
	} else {
		return -1;
	}
	p->stat = (p->stat & ~(PDI_MASKC)) | PDI_IBUSY | PDI_SETC(dc);
	return 0;
}

int pdi_getword(struct PDI_port *p, uint16_t *word)
{
	int dc = PDI_GETC(p->stat);
	int rc = PDI_GETR(p->stat);
	if(dc && rc && word) {
		*word = p->dat[0];
	} else {
		return -1;
	}
	dc--;
	rc--;
	for(int i = 0; i < dc; i++) p->dat[i] = p->dat[i+1];
	p->stat = (p->stat & ~(PDI_MASKC | PDI_MASKR)) | PDI_SETR(rc) | PDI_SETC(dc);
	return 0;
}

int pdi_getrxoverflow(struct PDI_port *p)
{
	int f;
	f = (p->stat & PDI_OVFLAG) != 0;
	p->stat &= ~PDI_OVFLAG;
	return f;
}
int pdi_isbusy(struct PDI_port *p)
{
	return (PDI_GETR(p->stat) < PDI_GETC(p->stat));;
}
int pdi_hasdata(struct PDI_port *p)
{
	return (PDI_GETR(p->stat) > 0);
}
int pdi_hasfree(struct PDI_port *p)
{
	return PDI_GETC(p->stat) < PDI_BUFLIMIT;
}
int pdi_process(struct PDI_port *p, struct timespec *pt, struct timespec const *now)
{
	int dc = PDI_GETC(p->stat);
	int rc = PDI_GETR(p->stat);
	if(rc < dc) {
		rc++;
		p->stat = (p->stat & ~(PDI_IBUSY | PDI_MASKC | PDI_MASKR)) | PDI_SETR(rc) | PDI_SETC(dc);
		return 1;
	}
	return 0;
}

