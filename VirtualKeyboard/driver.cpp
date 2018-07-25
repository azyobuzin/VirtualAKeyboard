#include <ntddk.h>
#include <wdf.h>
#include "trace.h"
#include "device.h"

extern "C" {
    // DriverEntry はマングリングできない; alloc_text は extern "C" にしか対応していない
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

    // WDF の儀式
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
