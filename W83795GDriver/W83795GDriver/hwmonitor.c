// hwmonitor.c, (C) 2013 Tyan Corp & D.Whobrey.
#include "driver.h"
#include <wdm.h>
#include <stdio.h>

#ifdef ALLOC_PRAGMA
//#pragma alloc_text(PAGE, HWMDriverIoctl)
#endif

extern WORD GetSlaveAdrs(void);
extern WORD ReadRegister(BYTE slvAdrs, BYTE reg);
extern WORD WriteRegister(BYTE slvAdrs, BYTE reg, BYTE val);
extern WORD SelectBankAndReadRegister(BYTE slvAdrs, BYTE reg, BYTE bank);
extern WORD SelectBankAndWriteRegister(BYTE slvAdrs, BYTE reg, BYTE bank, BYTE val);
extern DWORD SelectBankAndReadRegisterWord(BYTE slvAdrs, BYTE reg1, BYTE bank, BYTE reg2);
extern WORD SBTSIReadCommand(BYTE slvAdrs, BYTE reg, BYTE dts);
extern WORD SBTSIWriteCommand(BYTE slvAdrs, BYTE reg, BYTE dts, BYTE val);

BYTE hwmSlaveAddress = 0;
BYTE hwmPriorBank = 0;
int BMCState = 0;

#define Length_Cmd_ScanSuspend 4
BYTE Cmd_ScanSuspend[Length_Cmd_ScanSuspend] = {0xF8, 0x86, 0x80, 0x1E}; // 0x1E = 30d = 30 sec.
BYTE Cmd_ScanResumed[Length_Cmd_ScanSuspend] = {0xF8, 0x86, 0x00, 0x01};

VOID HWMDriverIoctl(__in WDFREQUEST request, __in PDEVICE_CONTEXT context) {
	NTSTATUS status = STATUS_SUCCESS;
	PHWM_REQUEST q = NULL, p = NULL;
	size_t inLen = HWM_IO_REQUEST_SIZE, outLen = HWM_IO_REQUEST_SIZE;	
	WORD ax=0; DWORD eax=0;
	BMC_REQUEST inR,outR;
	//PAGED_CODE();

	status = WdfRequestRetrieveOutputBuffer(request,outLen,&((PVOID)p),&outLen);
	if ((!NT_SUCCESS(status))||(outLen!=HWM_IO_REQUEST_SIZE)) {
		outLen = 0;
		goto exit;
	}

	status = WdfRequestRetrieveInputBuffer(request,inLen,&((PVOID)q),&inLen);
	if (!NT_SUCCESS(status)) {
		outLen = 0;
		goto exit;
	}

	CopyMemory(p,q,HWM_IO_REQUEST_SIZE);

	switch(q->Command) {
		case HWM_IO_CMD_GET_INFO: 
			// Fetch slave address.
			if((hwmSlaveAddress==0)||(BMCState==0)) {
				p->Status = HWM_STATUS_NOT_SAVED;
			} else {
				// Get I2CADDR, reg 0xfc, bank 0.
				DisableInts();
				ax=SelectBankAndReadRegister(hwmSlaveAddress, 0xfc, 0x0);
				EnableInts();
				if(ax>255) {
					p->Status = HWM_STATUS_BAD_I2CADDR;
					p->Value = ax>>8;
				} else {
					p->Status = HWM_STATUS_OK;
					p->Value = ax&0x7f;
				}
				p->Register=hwmSlaveAddress>>1;
			}
			break;
		case HWM_IO_CMD_STATE_SAVE:
			// Disable BMC SMBus scanning.
			CopyMemory(inR.Data,Cmd_ScanSuspend,Length_Cmd_ScanSuspend);
			inR.BlockLength=Length_Cmd_ScanSuspend;
			SendBmcRequest(&inR, &outR, context);
			BMCState=1;
			// Get slave address.
			if(hwmSlaveAddress==0) {
				DisableInts();
				hwmSlaveAddress = (BYTE)GetSlaveAdrs();
				EnableInts();
			}
			if(hwmSlaveAddress==0) {
				p->Status = HWM_STATUS_BAD_SLAVE_ADRS;
			} else {
				// Save current bank number.
				// Get current bank selected, reg 0x0, in any bank.
				DisableInts();
				ax=ReadRegister(hwmSlaveAddress,0x0);
				EnableInts();
				if(ax>255) {
					p->Status = HWM_STATUS_BAD_BANK_SELECT;
					p->Value = ax>>8;
				} else {
					p->Status = HWM_STATUS_OK;
					p->Value = ax&0xff;
					hwmPriorBank = ax&0xff;
				}
			}
			break;
		case HWM_IO_CMD_STATE_RESTORE:
			if(hwmSlaveAddress==0) {
				p->Status = HWM_STATUS_NOT_SAVED;
			} else {
				// Restore prior bank number.
				DisableInts();
				ax=WriteRegister(hwmSlaveAddress,0x0,hwmPriorBank);
				EnableInts();
				if(ax>255) {
					p->Status = HWM_STATUS_BAD_BANK_SELECT;
					p->Value = ax>>8;
				} else {
					p->Status = HWM_STATUS_OK;
					p->Value = 0;
				}
			}
			// Enable BMC SMBus scanning.
			CopyMemory(inR.Data,Cmd_ScanResumed,Length_Cmd_ScanSuspend);
			inR.BlockLength=Length_Cmd_ScanSuspend;
			SendBmcRequest(&inR, &outR, context);
			BMCState=0;
			break;
		case HWM_IO_CMD_REG_READ:
			if((hwmSlaveAddress==0)||(BMCState==0)) {
				p->Status = HWM_STATUS_NOT_SAVED;
			} else {
				DisableInts();
				if(q->Bank>=0xf0)
					ax=SBTSIReadCommand(hwmSlaveAddress, q->Register, q->Bank-0xf0);
				else 
					ax=SelectBankAndReadRegister(hwmSlaveAddress, q->Register, q->Bank);
				EnableInts();
				if(ax>255) {
					p->Status = HWM_STATUS_BAD_REG_READ;
					p->Value = ax>>8;
				} else {
					p->Status = HWM_STATUS_OK;
					p->Value = ax&0xff;
				}
			}
			break;
		case HWM_IO_CMD_REG_READ_WORD:
			if((hwmSlaveAddress==0)||(BMCState==0)) {
				p->Status = HWM_STATUS_NOT_SAVED;
			} else {
				DisableInts();
				eax=SelectBankAndReadRegisterWord(hwmSlaveAddress, q->Register, q->Bank, (BYTE)q->Value);
				EnableInts();
				if(eax&0xff00ff00) {
					p->Status = HWM_STATUS_BAD_REG_READ;
					p->Value = (eax>>8)&0xff;
				} else {
					p->Status = HWM_STATUS_OK;
					p->Value = ((eax>>8)&0xff00) + (eax&0xff);
				}
			}
			break;
		case HWM_IO_CMD_REG_WRITE:
			if((hwmSlaveAddress==0)||(BMCState==0)) {
				p->Status = HWM_STATUS_NOT_SAVED;
			} else {
				DisableInts();
				if(q->Bank>=0xf0)
					ax=SBTSIWriteCommand(hwmSlaveAddress, q->Register,  q->Bank-0xf0, (BYTE)q->Value);
				else 
					ax=SelectBankAndWriteRegister(hwmSlaveAddress, q->Register, q->Bank, (BYTE)q->Value);
				EnableInts();
				if(ax>255) {
					p->Status = HWM_STATUS_BAD_REG_WRITE;
					p->Value = ax>>8;
				} else {
					p->Status = HWM_STATUS_OK;
					p->Value = ax&0xff;
				}
			}
			break;
		default:
			p->Status = HWM_STATUS_CMD_INVALID;
	}
	outLen = HWM_IO_REQUEST_SIZE;
exit:
	WdfRequestSetInformation(request, outLen);
	WdfRequestCompleteWithInformation(request, status, outLen);
}
