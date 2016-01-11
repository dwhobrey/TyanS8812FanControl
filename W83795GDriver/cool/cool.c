// cool.c, (C) 2013 Tyan Corp & D.Whobrey.
// Adjusts register values of W83795G on S8812 using W83795GDriver.sys.
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <strsafe.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include "W83795GDriver.h"

HANDLE mutex;
HANDLE hndFile;
BYTE isverbose=0;

void ShowLastError(DWORD lastError) { 
	WCHAR buffer[200];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM 
		| FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,lastError,0,buffer,200,NULL );
	printf("%ld, %S\n",(long)lastError,buffer);
}

void ClearHR(PHWM_REQUEST p) {
	p->Status = 0;
	p->Command = 0;
	p->Bank = 0;
	p->Register = 0;
	p->Value = 0;
}

int OpenHWMDriver(void) {
	hndFile = CreateFileW(L"\\\\.\\W83795GDriver",
		GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (hndFile == INVALID_HANDLE_VALUE) {
		printf("\nUnable to open the device.\n");
		ShowLastError(GetLastError());
		return 1;
	}
	mutex = CreateMutex(NULL, FALSE, L"Global\\Access_SMBUS.HTP.Method");
    if (mutex == NULL) {
        printf("\nUnable to claim mutex.\n");
        return 1;
    }
	WaitForSingleObject(mutex,INFINITE);
	return 0;
}

void CloseHWMDriver(void) {
	if (!CloseHandle(hndFile)) {
		printf("\nFailed to close device.\n");
		ShowLastError(GetLastError());
	}
	ReleaseMutex(mutex);
	CloseHandle(mutex);
}

int performIO(PHWM_REQUEST pInBuffer,PHWM_REQUEST pOutBuffer) {
	DWORD inLen = HWM_IO_REQUEST_SIZE, outLen;
	int k;
	for(k=0;k<5;k++) {
		outLen = inLen;
		ClearHR(pOutBuffer);
		if(!DeviceIoControl(
			hndFile,        // Handle to device
			(DWORD)HWM_IO_REQUEST, // IO Control code for HWM Request
			pInBuffer,		// Buffer to driver.
			inLen,			// Length of buffer in bytes.
			pOutBuffer,		// Buffer from driver.
			outLen,			// Length of buffer in bytes.
			&outLen,		// Bytes placed in DataBuffer.
			NULL            // NULL means wait till op. completes.
			)||(pOutBuffer->Status!=0)) {
				continue;
		}
		return 0;
	}
	if(pOutBuffer->Status!=0) {
		printf("\nIoctl cmd failed:S=%x,V=%x.\n",pOutBuffer->Status,pOutBuffer->Value);
	} else {
		printf("\nIoctl failed.\n");
		ShowLastError(GetLastError());
	}
	return 1;
}

void ShowUsage(void) {
	printf("Tyan S8812 Hardware Monitor utility, ver 1.0.0.2, 16 Jan 2013.\n"
		"Run cool.exe as administrator from a dos box.\n"
		"Usage: cool [-verbose] {\n"
		"   -d [<num>]  Report or set duty cycle percentage, decimal integer: {5..99}.\n"
		"|  -r <bank> <reg>  Read register byte from bank.\n"
		"|  -t <bank> <tempMSBReg> <tempLSBReg> Read temperature from bank.\n"
		"|  -w <bank> <reg> <value>  Write value to registers.\n"
		"|  -i  Show information.\n"
		"|  -s  Dump Smart IV Temp/PWM data.\n"
		"}\n"
		"Note: register and value bytes are in hex format: {0..ff}.\n"
		"Setting bank>=f0, reads/writes to SB-TSI DTSx, x=0..7=bank-f0.\n"
		"See www.nuvoton.com W83795G manual for register details.\n"
	);
}

VOID __cdecl main(__in ULONG argc,__in_ecount(argc) PCHAR argv[]) {
	HWM_REQUEST inR,outR;
	ULONG argb=1;
	int duty=0,bank=0,reg=0,val=0;
	BYTE h,j,k,m,cmd=0,isdump=0,istemp=0,isduty=0,isinfo=0;
	float r=0.0,p=0.0,g=0.0;

	ClearHR(&inR);
	ClearHR(&outR);

	do {
		if ( argc < 2 || argv[argb][0] != '-') { ShowUsage(); return; }
			switch (argv[argb++][1]) {
			case 'd':
				if(argc<(argb+1)) {
					cmd = HWM_IO_CMD_REG_READ;
					bank=2;
					reg=0x88;
					isduty=1;
				} else {
					sscanf_s(argv[argb++],"%d",&duty);
					if(duty<5) duty=5;
					if(duty>99) duty=99;
					isduty=2;
				}
				break;
			case 'r':
				if(argc<(argb+2)) { ShowUsage(); return; }
				cmd = HWM_IO_CMD_REG_READ;
				sscanf_s(argv[argb++],"%x",&bank);
				sscanf_s(argv[argb++],"%x",&reg);
				break;
			case 't':
				if(argc<(argb+3)) { ShowUsage(); return; }
				cmd = HWM_IO_CMD_REG_READ_WORD;
				sscanf_s(argv[argb++],"%x",&bank);
				sscanf_s(argv[argb++],"%x",&reg);
				sscanf_s(argv[argb++],"%x",&val);
				istemp=1;
				break;
			case 'w':
				if(argc<(argb+3)) { ShowUsage(); return; }
				cmd = HWM_IO_CMD_REG_WRITE;
				sscanf_s(argv[argb++],"%x",&bank);
				sscanf_s(argv[argb++],"%x",&reg);
				sscanf_s(argv[argb++],"%x",&val);
				break;
			case 's':
				isdump=1;
				break;
			case 'i':
				isinfo=1;
				cmd = HWM_IO_CMD_GET_INFO;
				break;
			case 'v':
				isverbose=1;
				break;
			default: 
				ShowUsage();
				return;
		}
	} while(argb<argc);

	if(OpenHWMDriver()) return;

	if(isdump) {
		m=0x80;
		for(k=0;k<6;k++) {
			for(h=0;h<2;h++) {
				inR.Command = HWM_IO_CMD_STATE_SAVE;
				if(performIO(&inR,&outR)) goto exit;
				printf("%2x: ",m);
				inR.Command = HWM_IO_CMD_REG_READ;
				inR.Bank = 2;
				for(j=0;j<7;j++) {
					inR.Register = m++;
					inR.Value = 0;
					if(performIO(&inR,&outR)) printf("??/??? ");
					else printf("%02x/%03d ",outR.Value,outR.Value);
					
				}
				printf("\n");
				inR.Command = HWM_IO_CMD_STATE_RESTORE;
				performIO(&inR,&outR);
				++m;
			}
			printf("\n");
		}
	} else if(isduty==2) {
		p = (float)(duty/100.0);
		g = (float)(36.0*(1.0-p));
		for(k=0;k<6;k++) {
			if(k==4) continue;
			inR.Command = HWM_IO_CMD_STATE_SAVE;
			if(performIO(&inR,&outR)) goto exit;
			h = 0x88 + k*0x10;
			if(isverbose) printf("%x:",h);
			for(j=0;j<7;j++) {
				inR.Command = HWM_IO_CMD_REG_WRITE;
				inR.Bank = 2;
				inR.Register = h + j;
				r = (float)(255.0 * p + g * j);
				inR.Value = (BYTE)r;
				if(isverbose) printf("%02x/%03d ",inR.Value,inR.Value);
				if(performIO(&inR,&outR)) goto exit;
			}
			if(isverbose) printf("\n");
			inR.Command = HWM_IO_CMD_STATE_RESTORE;
			performIO(&inR,&outR);
		}
	} else {
		inR.Command = HWM_IO_CMD_STATE_SAVE;
		if(performIO(&inR,&outR)) goto exit;

		inR.Command = cmd;
		inR.Bank = (BYTE)bank;
		inR.Register = (BYTE)reg;
		inR.Value = (BYTE)val;
		performIO(&inR,&outR);

		if(istemp) {
			val = outR.Value;
			val>>=6;
			if(val&0x1) r += 0.25;
			if(val&0x2) r += 0.5;
			val>>=2; val&=0xff;
			if(val&0x80) val=1-(val&0x7f);
			r += (float)val;
			printf("Temp for %x = %g.\n",reg,r);
		} else if(isduty) {
			val=(100*(2+outR.Value))/255;
			printf("Smart Fan IV duty cycle:%02d%%.\n",val);
		} else if(isinfo) {
			printf("W83795G SMBus slave address:%x.\n",outR.Register);
			printf("Register I2CADDR:0xfc value:%x.\n",outR.Value);
		} else {
			printf("Status=%x,Command=%x,Bank=%x,Register=%x,Value=%x.\n",
				outR.Status,outR.Command,outR.Bank,outR.Register,outR.Value);
		}

		inR.Command = HWM_IO_CMD_STATE_RESTORE;
		performIO(&inR,&outR);
	}

	exit:
	CloseHWMDriver();
}
