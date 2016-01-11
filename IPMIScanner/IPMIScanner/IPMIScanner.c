// IPMIScanner.c, (C) 2013  D.Whobrey.
// Scans Temp & Fan sensor values via bmc.

// Core routines extracted from isensor.c and ipmicmd.c from ipmiutil.
// Original Author:  arcress at users.sourceforge.net.

/*
The BSD License 

Copyright (c) 2002-2006 Intel Corporation. 
Copyright (c) 2009 Kontron America, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

a.. Redistributions of source code must retain the above copyright notice, 
this list of conditions and the following disclaimer. 
b.. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution. 
c.. Neither the name of Kontron nor the names of its contributors 
may be used to endorse or promote products derived from this software 
without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 

#define uchar   unsigned char
#define uint32  unsigned int
#define uint64  unsigned long

#ifndef ushort
#define ushort  unsigned short
#endif

#define PUBLIC_BUS      0
#define PRIVATE_BUS  0x03
#define BMC_SA       0x20
#define BMC_LUN         0
#define SMS_LUN         2
#define HSC_SA       0xC0
#define ME_SA        0x2C
#define ME_BUS       0x06

#define ADDR_SMI     1
#define ADDR_IPMB    2

#define ACCESS_OK    0
#define ERR_BAD_LENGTH 1

#define NETFN_SEVT   0x04  // sensor/event
#define NETFN_APP    0x06  // application
#define NETFN_STOR   0x0a  // storage

#define MAX_BUFFER_SIZE     255
#define CMDMASK				0xff 
#define GET_POWERON_HOURS   0x0F
#define GET_DEVICE_ID       (0x01 | (NETFN_APP << 8))
#define GET_DEVSDR_INFO		(0x20 | (NETFN_SEVT << 8))
#define GET_SDR_REPINFO		(0x20 | (NETFN_STOR << 8))
#define GET_DEVICE_SDR		(0x21 | (NETFN_SEVT << 8))

#define RESERVE_DEVSDR_REP	(0x22 | (NETFN_SEVT << 8))
#define RESERVE_SDR_REP     (0x22 | (NETFN_STOR << 8))
#define GET_SDR				(0x23 | (NETFN_STOR << 8))
#define GET_SENSOR_READING	(0x2D | (NETFN_SEVT << 8))

extern int ipmi_close_ms(void);

extern int ipmi_cmdraw_ms(uchar cmd, uchar netfn, uchar lun, uchar sa, 
						  uchar bus, uchar *pdata, int sdata, 
						  uchar *presp, int *sresp, uchar *pcc);

extern void SaveRegistryKey(const char* p);

#define MIN_SDR_SZ  8 
#define SZCHUNK 16

DWORD scanDelay=5000;
static int isverbose=0;
static int fdevsdrs = 0;
static int fReserveOK = 1;
static int fDoReserve = 1;
static int nsdrs = 0;
static uchar resid[2] = {0,0};

typedef struct {
	ushort recid;
	uchar  sdrver;  /*usu. 0x51 = v1.5*/
	uchar  rectype; /* 01, 02, 11, 12, c0 */
	uchar  reclen;
	uchar  sens_ownid;
	uchar  sens_ownlun;
	uchar  sens_num;
	uchar  entity_id;
	uchar  entity_inst;
	uchar  sens_init; 
	uchar  sens_capab; 
	uchar  sens_type; 
	uchar  ev_type;
	uchar  data1[6];
	uchar  sens_units;
	uchar  sens_base;
	uchar  sens_mod;
	uchar  linear;
	uchar  m;
	uchar  m_t;
	uchar  b;
	uchar  b_a;
	uchar  a_ax;
	uchar  rx_bx;
	uchar  flags;
	uchar  nom_reading;
	uchar  norm_max;
	uchar  norm_min;
	uchar  sens_max_reading;
	uchar  sens_min_reading;
	uchar  data3[11];
	uchar  id_strlen;
	uchar  id_string[16];
} SDR01REC; 

typedef struct {
	uchar adrtype;
	uchar sa;
	uchar bus;
	uchar lun;
	uchar capab;
} mc_info;

mc_info  bmc = { ADDR_SMI,  BMC_SA, PUBLIC_BUS, BMC_LUN, 0x8F }; /*BMC via SMI*/
mc_info  mc2 = { ADDR_IPMB, BMC_SA, PUBLIC_BUS, BMC_LUN, 0x4F }; /*IPMB target*/
mc_info  *mc  = &bmc;

uchar my_devid[20] = {0,0,0,0,0,0,0,0}; /*saved devid, only needs 16 bytes*/  

/* 
* ipmi_cmd_mc()
*
* This routine can be used to invoke IPMI commands that are not
* already pre-defined in the ipmi_cmds array.
* It invokes whichever driver-specific routine is needed (ia, mv, etc.).
* Parameters:
* uchar cmd     (input): IPMI Command
* uchar netfn   (input): IPMI NetFunction
* uchar sa      (input): IPMI Slave Address of the MC
* uchar bus     (input): BUS  of the MC
* uchar lun     (input): IPMI LUN
* uchar *pdata  (input): pointer to ipmi data
* int sdata   (input): size of ipmi data
* uchar *presp (output): pointer to response data buffer
* int *sresp   (input/output): on input, size of response buffer,
*                              on output, length of response data
* uchar *cc    (output): completion code
*/
int ipmi_cmd_mc(ushort icmd, uchar *pdata, int sdata, uchar *presp, int *sresp, uchar *pcc) {
	uchar cmd, netfn;
	int rv;
	cmd = icmd & CMDMASK;
	netfn = (icmd & 0xFF00) >> 8;
	if (sdata > 255) return(ERR_BAD_LENGTH);
	*pcc = 0;
	rv = ipmi_cmdraw_ms(cmd, netfn, mc->lun, mc->sa, mc->bus,pdata, sdata, presp, sresp, pcc);
	return(rv);
}

int ipmi_getdeviceid(uchar *presp, int sresp) {
	int rc, i; uchar cc;
	/* check that sresp is big enough (default is 15 bytes for Langley)*/
	if (sresp < 15) return(ERR_BAD_LENGTH);
	rc = ipmi_cmd_mc(GET_DEVICE_ID, NULL, 0, presp,&sresp, &cc);
	if (rc != ACCESS_OK) return(rc);
	if (cc != 0) return(cc);
	i = sresp;
	if (i > sizeof(my_devid)) i = sizeof(my_devid);
	memcpy(my_devid,presp,i); /* save device id for later use */
	return(ACCESS_OK);   /* success */
}

void ipmi_set_mc(uchar bus, uchar sa, uchar lun, uchar atype) 
{
	mc = &mc2;
	mc->bus  = bus;
	mc->sa   = sa;
	mc->lun  = lun;
	mc->adrtype = atype; /* ADDR_SMI or ADDR_IPMB */
	return;
}

void ipmi_restore_mc(void)
{
	mc = &bmc;
	return;
}

int GetSDRRepositoryInfo(int *nret) {
	int rc,nSDR,sresp = MAX_BUFFER_SIZE;
	ushort cmd;
	uchar cc = 0;
	uchar resp[MAX_BUFFER_SIZE];
	memset(resp,0,6);  /* init first part of buffer */
	if (nret != NULL) *nret = 0;
	if (fdevsdrs) cmd = GET_DEVSDR_INFO;
	else cmd = GET_SDR_REPINFO;
	rc = ipmi_cmd_mc(cmd, NULL, 0, resp,&sresp, &cc);
	/* some drivers return cc in rc */
	if ((rc == 0xc1) || (rc == 0xd4)) cc = rc; 
	else if (rc != 0) return(rc);
	if (cc != 0) {
		if ((cc == 0xc1) ||  /*0xC1 (193.) means unsupported command */
			(cc == 0xd4))    /*0xD4 means insufficient privilege (Sun/HP)*/
		{
			/* Must be reporting wrong bit for fdevsdrs, 
			* so switch & retry */
			if (fdevsdrs) { 
				fdevsdrs = 0;
				cmd = GET_SDR_REPINFO;
			} else {  
				fdevsdrs = 1;
				cmd = GET_DEVSDR_INFO;
			}
			sresp = MAX_BUFFER_SIZE;
			rc = ipmi_cmd_mc(cmd, NULL, 0, resp,&sresp, &cc);
			if (rc != ACCESS_OK) return(rc);
			if (cc != 0) return(cc);
		} else return(cc);
	}

	if (fdevsdrs) {
		nSDR = resp[0]; 
		fReserveOK = 1;
	} else {
		nSDR = resp[1] + (resp[2] << 8);
		if ((resp[13] & 0x02) == 0) fReserveOK = 0;
		else fReserveOK = 1;
	}
	if (nret != NULL) *nret = nSDR;
	return(0);
} 

int GetSensorReading(uchar sens_num, void *psdr, uchar *sens_data) {
	uchar resp[MAX_BUFFER_SIZE];
	int sresp = MAX_BUFFER_SIZE;
	uchar inputData[6];
	SDR01REC *sdr = NULL; //DEBUG: was SDR02REC.
	int mc;
	int rc;
	uchar cc = 0;
	uchar lun, chan = 0;

	if (psdr != NULL) {
		sdr = (SDR01REC *)psdr;//DEBUG: was SDR02REC.
		mc = sdr->sens_ownid;
		if (mc != BMC_SA) {  /* not BMC, e.g. HSC or ME sensor */
			uchar a = ADDR_IPMB;
			if (mc == HSC_SA) a = ADDR_SMI;
			chan = (sdr->sens_ownlun & 0xf0) >> 4;
			lun = (sdr->sens_ownlun & 0x03);
			ipmi_set_mc(chan,(uchar)mc, lun,a);
		}
	} else mc = BMC_SA;
	inputData[0] = sens_num;
	rc = ipmi_cmd_mc(GET_SENSOR_READING,inputData,1, resp,&sresp,&cc);
	if (mc != BMC_SA) ipmi_restore_mc();
	if ((rc == 0) && (cc != 0)) {
		rc = cc;
	}
	if (rc != 0) return(rc);

	if (resp[1] & 0x20) { /* init state, reading invalid */
		sens_data[1] = resp[1];
		sens_data[2] = 0x40;  /*set bit num for init state*/
	} else {     /*valid reading, copy it*/
		/* only returns 4 bytes, no matter what type */
		memcpy(sens_data,resp,4);
	}
	return(0);
} 

int sdr_get_reservation(uchar *res_id, int fdev)
{
	int sresp;
	uchar resp[MAX_BUFFER_SIZE];
	uchar cc = 0;
	ushort cmd;
	int rc = -1;

	if (fDoReserve == 1) {
		fDoReserve = 0; /* only reserve SDR the first time */
		sresp = sizeof(resp);;
		if (fdev) cmd = RESERVE_DEVSDR_REP;
		else cmd = RESERVE_SDR_REP;
		rc = ipmi_cmd_mc(cmd, NULL, 0, resp, &sresp, &cc);
		if (rc == 0 && cc != 0) rc = cc;
		if (rc == 0) {  /* ok, so set the reservation id */
			resid[0] = resp[0]; 
			resid[1] = resp[1];
		}
		/* A reservation is cancelled by the next reserve request. */
	} else rc = 0;
	/* if not first time, or if error, return existing resid. */
	res_id[0] = resid[0];
	res_id[1] = resid[1];
	return(rc);
} 

int GetSDR(int r_id, int *r_next, uchar *recdata, int srecdata, int *rlen) {
	int sresp;
	uchar resp[MAX_BUFFER_SIZE+SZCHUNK];
	uchar respchunk[SZCHUNK+10];
	uchar inputData[6];
	uchar cc = 0;
	int rc = -1;
	int  chunksz, thislen, off;
	int reclen;
	ushort cmd;
	uchar resv[2] = {0,0};

	chunksz = SZCHUNK;
	reclen = srecdata; /*max size of SDR record*/
	off = 0;
	*rlen = 0;
	*r_next = 0xffff;  /*default*/
	if (fReserveOK) 
		rc = sdr_get_reservation(resv,fdevsdrs);
	if (fdevsdrs) cmd = GET_DEVICE_SDR;
	else cmd = GET_SDR;
	if (reclen == 0xFFFF) {  /* get it all in one call */
		inputData[0] = resv[0];     /*res id LSB*/
		inputData[1] = resv[1];     /*res id MSB*/
		inputData[2] = r_id & 0x00ff;        /*record id LSB*/
		inputData[3] = (r_id & 0xff00) >> 8;	/*record id MSB*/
		inputData[4] = 0;      /*offset */
		inputData[5] = 0xFF;  /*bytes to read, ff=all*/
		sresp = sizeof(resp);;
		rc = ipmi_cmd_mc(cmd, inputData, 6, recdata, &sresp,&cc);
		/* This will usually return cc = 0xCA (invalid length). */
		reclen = sresp;
		*r_next = recdata[0] + (recdata[1] << 8);
	} else    /* if (reclen > chunksz) do multi-part chunks */
		for (off = 0; off < reclen; )
		{
			thislen = chunksz;
			if ((off+chunksz) > reclen) thislen = reclen - off;
			inputData[0] = resv[0];     /*res id LSB*/
			inputData[1] = resv[1];     /*res id MSB*/
			inputData[2] = r_id & 0x00ff; 	/*record id LSB*/
			inputData[3] = (r_id & 0xff00) >> 8;	/*record id MSB*/
			inputData[4] = (uchar)off;      /*offset */
			inputData[5] = (uchar)thislen;  /*bytes to read, ff=all*/
			sresp = sizeof(respchunk);
			rc = ipmi_cmd_mc(cmd, inputData, 6, respchunk, &sresp,&cc);
			if (off == 0 && cc == 0xCA && thislen == SZCHUNK) { 
				/* maybe shorter than SZCHUNK, try again */
				chunksz = 0x06;
				continue;
			}
			if (off > chunksz) {
				/* already have first part of the SDR, ok to truncate */
				if (rc == -3) { /* if LAN_ERR_RECV_FAIL */
					sresp = 0;
					rc = 0;
				}
				if (cc == 0xC8 || cc == 0xCA) { /* length errors */
					/* handle certain MCs that report wrong length,
					* at least use what we already have (sresp==0) */
					cc = 0;
				}
			}
			if (rc != ACCESS_OK) return(rc);
			if (cc != 0) return(cc);
			/* if here, successful, chunk was read */
			if (sresp < (thislen+2)) {
				/* There are some SDRs that may report the wrong length, and
				* return less bytes than they reported, so just truncate. */
				if (sresp >= 2) thislen = sresp - 2;
				else thislen = 0;
				reclen = off + thislen;  /* truncate, stop reading */
			}
			/* successful */
			memcpy(&resp[off],&respchunk[2],thislen);
			if (off == 0 && sresp >= 5) {
				*r_next = respchunk[0] + (respchunk[1] << 8);
				reclen = respchunk[6] + 5;  /*get actual record size*/
				if (reclen > srecdata) {
					reclen = srecdata; /*truncate*/
				}
			}
			off += thislen;
			*rlen = off;
		}
		memcpy(recdata,&resp[0],reclen);
		*rlen = reclen;
		return(rc);
}

static double expon(int x, int y) {
	double res;
	int i;
	/* compute exponent: x to the power y */
	res = 1;
	if (y > 0) {
		for (i = 0; i < y; i++) res = res * x;
	} else if (y < 0) {
		for (i = 0; i > y; i--) res = res / x;
	} /* else if if (y == 0) do nothing, res=1 */
	return(res);
}

double RawToFloat(uchar raw, uchar *psdr) {
	double floatval;
	int m, b, a;
	uchar  ax; 
	int  rx, b_exp;
	SDR01REC *sdr;
	int signval;
	sdr = (SDR01REC *)psdr;
	floatval = (double)raw;  /*default*/
	if (sdr->rectype == 0x01) {  /* SDR rectype == full */
		m = sdr->m + ((sdr->m_t & 0xc0) << 2);
		b = sdr->b + ((sdr->b_a & 0xc0) << 2);
		if (b & 0x0200) b = (b - 0x0400);  /*negative*/
		if (m & 0x0200) m = (m - 0x0400);  /*negative*/
		rx = (sdr->rx_bx & 0xf0) >> 4;
		if (rx & 0x08) rx = (rx - 0x10); /*negative, fix sign w ARM compilers*/
		a = (sdr->b_a & 0x3f) + ((sdr->a_ax & 0xf0) << 2);
		ax = (sdr->a_ax & 0x0c) >> 2;
		b_exp = (sdr->rx_bx & 0x0f);
		if (b_exp & 0x08) b_exp = (b_exp - 0x10);  /*negative*/
		if ((sdr->sens_units & 0xc0) == 0) {  /*unsigned*/
			floatval = (double)raw;
		} else {  /*signed*/
			if (raw & 0x80) signval = (raw - 0x100);
			else signval = raw;
			floatval = (double)signval;
		}
		floatval *= (double) m;
		floatval += (b * expon (10,b_exp));
		floatval *= expon (10,rx);
		if(sdr->linear==7) { /*invert 1/x*/
			if (raw != 0) floatval = 1 / floatval;
		}
	}
	return(floatval);
}

#define MaxSensorNum 60
#define MaxSensorText 40
#define NumSensorNames (sizeof(SensorNameMap)/sizeof(char*))
#define RegistryBuffSize (NumSensorNames*MaxSensorText)
char* SensorNameMap[] = {
	"CPU0 TEMP","Cpu 0",
	"CPU1 TEMP","Cpu 1",
	"CPU2 TEMP","Cpu 2",
	"CPU3 TEMP","Cpu 3",
	"AMBIENT","Ambient",
	"PCI_INLET_AREA","PCI",
	"RD5690_CASE_TEMP","RD5690",
	"SAS_CASE_TEMP","SAS",
	"CPU0_MOS_AREA","MOS 0",
	"CPU1_MOS_AREA","MOS 1",
	"CPU2_MOS_AREA","MOS 2",
	"CPU3_MOS_AREA","MOS 3",
	"CPU0 FAN","CFan 0",
	"CPU1 FAN","CFan 1",
	"CPU2 FAN","CFan 2",
	"CPU3 FAN","CFan 3",
	"FRONT FAN1","FFan 1",
	"FRONT FAN2","FFan 2",
	"FRONT FAN3","FFan 3",
	"FRONT FAN4","FFan 4",
	"REAR FAN1","RFan 1",
	"REAR FAN2","RFan 2" 
};
uchar SensorNameCache[MaxSensorNum] = {0};
char RegistryBuffer[RegistryBuffSize];

char* SensorLabel(uchar num,char* id) {
	uchar k;
	if(num>=MaxSensorNum) return id;
	k=SensorNameCache[num];
	if(k>0) return SensorNameMap[k];
	for(k=0;k<NumSensorNames;k+=2) {
		if(strcmp(id,SensorNameMap[k])==0) {
			SensorNameCache[num]=k+1;
			return SensorNameMap[k+1];
		}
	}
	return id;
}

int ShowSDR(uchar *sdr,char*buffer) {
	SDR01REC *sdr01;
	char *s;
	double val;
	int len,ilen,ioff,rc;
	uchar sens[4];
	char idstr[32];
	len = sdr[4] + 5;
	memset(sens,0,4);
	if(sdr[3]!=0x01) return 0; 
	sdr01 = (SDR01REC *)sdr;
	ioff = 48;
	if (ioff > len) return 0;
	ilen = len - ioff;
	if (ilen >= sizeof(idstr)) ilen = sizeof(idstr) - 1;
	memcpy(idstr,&sdr[ioff],ilen);
	idstr[ilen] = 0;
	rc = GetSensorReading(sdr01->sens_num,sdr01,sens);
	if ((rc != 0)||((sens[1] & 0x20) != 0)) val = 0;
	else val = RawToFloat(sens[0],sdr);
	s=SensorLabel(sdr01->sens_num,idstr);
	return sprintf(buffer,"%s,%.0f;\n",s,val);
}

int ShowPowerOnHours(char* buffer) {
	uchar cc,resp[MAX_BUFFER_SIZE];
	int sresp = MAX_BUFFER_SIZE,rc = -1,i;
	unsigned long hrs;
	sresp = MAX_BUFFER_SIZE;
	memset(resp,0,6);  /* default response size is 5 */
	rc = ipmi_cmd_mc(GET_POWERON_HOURS, NULL, 0, resp, &sresp, &cc);
	if (rc == 0 && cc == 0) {
		/* show the hours (32-bits) */
		hrs = resp[1] | (resp[2] << 8) | (resp[3] << 16) | (resp[4] << 24);
		if (resp[0] == 0) /*avoid div-by-zero*/ i = 1;
		else if (resp[0] == 60) /*normal*/ i = 1;
		else {
			i = 60 / resp[0];
			hrs = hrs / i;
		}
		return sprintf(buffer,"Hours,%ld\n",hrs);
	}
	return 0;
}

int ScanSensors(void) {
	char *p;
	int k,sz,ret,recid,recnext;
	uchar sdrdata[MAX_BUFFER_SIZE],devrec[16];
	ret = ipmi_getdeviceid(devrec,sizeof(devrec));
	if (ret != 0) goto do_exit;
	ret = GetSDRRepositoryInfo(&nsdrs);
	if (ret == 0 && nsdrs == 0) goto do_exit;

	for(;;) {
		p=RegistryBuffer;
		recid = 0;
		while (recid != 0xffff) {
			ret = GetSDR(recid,&recnext,sdrdata,sizeof(sdrdata),&sz);
			if (ret != 0) {
				if (ret > 0) {  /* ret is a completion code error */
					if (ret == 0xC5) {  /* lost Reservation ID, retry */
						fDoReserve = 1;  /* get a new SDR Reservation ID */
						ret = GetSDR(recid,&recnext,sdrdata,sizeof(sdrdata),&sz);
					}
				} 
				if (sz < MIN_SDR_SZ) break;
			} else {
				if (sz < MIN_SDR_SZ) goto NextSdr;
				if ((sdrdata[3]== 0x03)||(sdrdata[3] == 0xc0)
					||((sdrdata[12] != 1)&&(sdrdata[12]!=4))
					) goto NextSdr;
				k=ShowSDR(sdrdata,p);
				if(k>0)	p+=k;
			}
	NextSdr: 
			if (recnext == recid) recid = 0xffff;
			else recid = recnext;
		} 
		k=ShowPowerOnHours(p);
		if(k>0)	p+=k;
		p[k]='\0';
		if(isverbose) printf(RegistryBuffer);
		SaveRegistryKey(RegistryBuffer);
		Sleep(scanDelay);
	}

do_exit:
	ipmi_close_ms();
	return(ret);  
}

void ShowUsage(void) {
	printf("IPMI Scanner utility, ver 1.0.0.0, 16 Jan 2013.\n"
		"Run IPMIScanner.exe as administrator from task scheduler.\n"
		"Usage: IPMIScanner [options]\n"
		"   -v  Show results on stdout.\n"
		"   -t seconds  Time between scans.\n"
	);
}

void main(int argc,char* argv[]) {
	int argb=1;
	long secs=5;
	do {
		if ( argc > 1) {
			switch (argv[argb++][1]) {
			case 't':
					sscanf_s(argv[argb++],"%ld",&secs);
					if(secs<1) secs=1; else if(secs>60) secs=60;
					scanDelay=secs*1000;
				break;
			case 'v':
				isverbose=1;
				break;
			default: 
				ShowUsage();
				return;
			}
		}
	} while(argb<argc);
	ScanSensors();
}
