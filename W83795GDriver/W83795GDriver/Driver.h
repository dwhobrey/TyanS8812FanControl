// driver.h, W8, (C) 2013 Tyan Corp & D.Whobrey.
#define INITGUID

#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include <conio.h>
#include <intrin.h>
#include "device.h"
#include "queue.h"
#include "trace.h"
#include "W83795GDriver.h"
#include "kcsbmc.h"

#define HWM_DRIVER_DEVICE_NAME L"W83795GDriver"
#define HWM_DRIVER_DEVICE_PATH L"\\Device\\" HWM_DRIVER_DEVICE_NAME
#define DOS_DEVICE_PATH L"\\DosDevices\\"  HWM_DRIVER_DEVICE_NAME

#define WAIT_1_MS 1000 // in uSecs
#define WAIT_5_MS 5000 // in uSecs
#define WAIT_10_MS 10000 // in uSecs

#define DisableInts() (_disable())
#define EnableInts() (_enable())
#define CopyMemory(dst,src,count) RtlCopyMemory((dst),(src),(count))
#define ZeroMemory(dst,count) RtlZeroMemory((dst),(count))

// WDFDRIVER Events
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD W83795GDriverEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP W83795GDriverEvtDriverContextCleanup;

extern VOID HWMDriverIoctl(__in WDFREQUEST Request, __in PDEVICE_CONTEXT context);