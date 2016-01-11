// queue.h, W8, (C) 2013 Tyan Corp & D.Whobrey.
NTSTATUS W83795GDriverQueueInitialize( _In_ WDFDEVICE hDevice);

// Events from the IoQueue object
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL W83795GDriverEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP W83795GDriverEvtIoStop;

