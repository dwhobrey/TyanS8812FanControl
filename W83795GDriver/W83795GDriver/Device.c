// device.c, W8, (C) 2013 Tyan Corp & D.Whobrey.
#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, W83795GDriverCreateDevice)
#endif

/*++
Routine Description:
    Worker routine called to create a device and its software resources.
Arguments:
    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.
Return Value:
    NTSTATUS
--*/
NTSTATUS W83795GDriverCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    ) {
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;
    UNICODE_STRING ntDeviceName;
    UNICODE_STRING win32DeviceName;
    WDF_FILEOBJECT_CONFIG fileConfig;

    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

	WDF_FILEOBJECT_CONFIG_INIT(
                    &fileConfig,
                    WDF_NO_EVENT_CALLBACK, 
                    WDF_NO_EVENT_CALLBACK, 
                    WDF_NO_EVENT_CALLBACK // not interested in Cleanup
                    );
    
    // Let the framework complete create and close
    fileConfig.AutoForwardCleanupClose = WdfFalse;
    
    WdfDeviceInitSetFileObjectConfig(DeviceInit,
                                     &fileConfig,
                                     WDF_NO_OBJECT_ATTRIBUTES);
    // Create a named deviceobject so that legacy applications can talk to us.
    // Since we are naming the object, we wouldn't able to install multiple
    // instance of this driver. Please note that as per PNP guidelines, we
    // should not name the FDO or create symbolic links. We are doing it because
    // we have a legacy APP that doesn't know how to open an interface.
    RtlInitUnicodeString(&ntDeviceName, HWM_DRIVER_DEVICE_PATH);
    
    status = WdfDeviceInitAssignName(DeviceInit,&ntDeviceName);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    WdfDeviceInitSetDeviceType(DeviceInit, HWM_DRIVER_TYPE);

    // Call this if the device is not holding a pagefile
    // crashdump file or hibernate file.
    WdfDeviceInitSetPowerPageable(DeviceInit);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        // Get a pointer to the device context structure that we just associated
        // with the device object. We define this structure in the device.h
        // header file. HWMDriverGetDeviceContext is an inline function generated by
        // using the WDF_DECLARE_CONTEXT_TYPE_WITH_NAME macro in device.h.
        // This function will do the type checking and return the device context.
        // If you pass a wrong object handle it will return NULL and assert if
        // run under framework verifier mode.
        deviceContext = HWMDriverGetDeviceContext(device);

        // Initialize the context.

		// Create a device interface so that application can find and talk to us.
		RtlInitUnicodeString(&win32DeviceName, DOS_DEVICE_PATH);
		status = WdfDeviceCreateSymbolicLink(device,&win32DeviceName);
		if (!NT_SUCCESS(status)) {
			return status;
		}

        // Create a device interface so that applications can find and talk to us.
        status = WdfDeviceCreateDeviceInterface(
            device,
            &GUID_DEVINTERFACE_W83795GDriver,
            NULL // ReferenceString
            );

        if (NT_SUCCESS(status)) {
            // Initialize the I/O Package and any Queues
            status = W83795GDriverQueueInitialize(device);
        }
    }

    return status;
}


