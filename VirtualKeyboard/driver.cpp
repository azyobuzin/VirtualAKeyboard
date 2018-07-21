#include <ntddk.h>
#include <wdf.h>
#include "trace.h"
#include "device.h"

extern "C" {
	// DriverEntry ‚Íƒ}ƒ“ƒOƒŠƒ“ƒO‚Å‚«‚È‚¢; alloc_text ‚Í extern "C" ‚É‚µ‚©‘Î‰ž‚µ‚Ä‚¢‚È‚¢
	DRIVER_INITIALIZE DriverEntry;
	EVT_WDF_DRIVER_DEVICE_ADD VirtualKeyboardDriverEvtDeviceAdd;
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, VirtualKeyboardDriverEvtDeviceAdd)
#endif

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	TraceEnterFunc();

	WDF_DRIVER_CONFIG config;
	WDF_DRIVER_CONFIG_INIT(&config, VirtualKeyboardDriverEvtDeviceAdd);

	// WDF ‚Ì‹VŽ®
	NTSTATUS status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);

	if (!NT_SUCCESS(status))
		TraceErrorStatus("WdfDriverCreate", status);

	return status;
}

NTSTATUS VirtualKeyboardDriverEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit) {
	UNREFERENCED_PARAMETER(Driver);
	PAGED_CODE();

	return VirtualKeyboardCreateDevice(DeviceInit);
}
