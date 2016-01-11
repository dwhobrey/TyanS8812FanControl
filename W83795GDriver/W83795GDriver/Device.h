// device.h, W8, (C) 2013 Tyan Corp & D.Whobrey.
#include "public.h"

// The device context performs the same job as
// a WDM device extension in the driver frameworks
typedef struct _DEVICE_CONTEXT {	
	KTIMER mesgTimer; // Imb request timeout generating timer object pointer.
	KDPC mesgTimerDpc; // pointer to the IMB request related timeout processing DPC.
	BYTE timeOutFlag; // IMB request related timeout indicating flag.
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, HWMDriverGetDeviceContext)

// Function to initialize the device and its callbacks
NTSTATUS W83795GDriverCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

