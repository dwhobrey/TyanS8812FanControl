// W83795GDriver.h, (C) 2013 Tyan Corp & D.Whobrey.

#ifndef __W83795GDRIVER_H
#define __W83795GDRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define HWM_DRIVER_TYPE 0x00009C4A // Device Type.
#define HWM_IO_REQUEST_SIZE sizeof(HWM_REQUEST)

#define HWM_IO_REQUEST CTL_CODE(HWM_DRIVER_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HWM_IO_CMD_GET_INFO			0
#define HWM_IO_CMD_STATE_SAVE		1
#define HWM_IO_CMD_STATE_RESTORE	2
#define HWM_IO_CMD_REG_READ			3
#define HWM_IO_CMD_REG_WRITE		4
#define HWM_IO_CMD_REG_READ_WORD	5

#define HWM_STATUS_OK				0
#define HWM_STATUS_CMD_INVALID		1
#define HWM_STATUS_BAD_SLAVE_ADRS	2
#define HWM_STATUS_NOT_SAVED		3
#define HWM_STATUS_BAD_I2CADDR		4
#define HWM_STATUS_BAD_BANK_SELECT	5
#define HWM_STATUS_BAD_REG_READ		6
#define HWM_STATUS_BAD_REG_WRITE	7

typedef unsigned short WORD;

typedef struct _HWM_REQUEST {
	BYTE Status;
	BYTE Command;
	BYTE Bank;
	BYTE Register;
	WORD Value;
} HWM_REQUEST, *PHWM_REQUEST;

#ifdef __cplusplus
}
#endif

#endif // __W83795GDRIVER_H
