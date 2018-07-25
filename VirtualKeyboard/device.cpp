#include <ntddk.h>
#include <wdf.h>
#include <vhf.h>
#include "trace.h"
#include "device.h"

// 初回待ち時間
#define KEY_DOWN_TIMER_DUE_TIME_MS 10000

// 定期入力間隔
#define KEY_DOWN_TIMER_INTERVAL_MS 1000

// キーを離すまでの時間
#define KEY_UP_TIMER_DUE_TIME_MS 200

// 終了するまでの回数
#define LOOP_COUNT 10

extern "C" {
    EVT_WDF_OBJECT_CONTEXT_CLEANUP VirtualKeyboardDeviceEvtCleanupCallback;
    EVT_VHF_CLEANUP VirtualKeyboardEvtVhfCleanup;
    NTSTATUS VirtualKeyboardDeviceCreateKeyDownTimer(WDFDEVICE);
    EVT_WDF_TIMER VirtualKeyboardDeviceKeyDownTimerEvtTimerFunc;
    NTSTATUS VirtualKeyboardDeviceCreateKeyUpTimer(WDFDEVICE);
    EVT_WDF_TIMER VirtualKeyboardDeviceKeyUpTimerEvtTimerFunc;
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, VirtualKeyboardCreateDevice)
#pragma alloc_text (PAGE, VirtualKeyboardDeviceEvtCleanupCallback)
#pragma alloc_text (PAGE, VirtualKeyboardDeviceCreateKeyDownTimer)
#pragma alloc_text (PAGE, VirtualKeyboardDeviceCreateKeyUpTimer)
#pragma alloc_text (PAGE, VirtualKeyboardDeviceKeyUpTimerEvtTimerFunc)
#endif

typedef struct _VIRTUAL_KEYBOARD_DEVICE_CONTEXT {
    VHFHANDLE VhfHandle;
    WDFTIMER KeyDownTimer;
    WDFTIMER KeyUpTimer;
    BYTE Counter;
} VIRTUAL_KEYBOARD_DEVICE_CONTEXT, *PVIRTUAL_KEYBOARD_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(VIRTUAL_KEYBOARD_DEVICE_CONTEXT, VirtualKeyboardGetDeviceContext);

static UCHAR ReportDescriptor[27] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x09, 0x04,                    //   USAGE (Keyboard a and A)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x07,                    //   REPORT_COUNT (7)
    0x81, 0x01,                    //   INPUT (Cnst,Ary,Abs)
    0xc0                           // END_COLLECTION
};

NTSTATUS VirtualKeyboardCreateDevice(PWDFDEVICE_INIT DeviceInit) {
    PAGED_CODE();

    TraceEnterFunc();

    // デバイス作成
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, VIRTUAL_KEYBOARD_DEVICE_CONTEXT);
    deviceAttributes.EvtCleanupCallback = VirtualKeyboardDeviceEvtCleanupCallback;

    WDFDEVICE device;
    NTSTATUS status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (!NT_SUCCESS(status)) {
        TraceErrorStatus("WdfDeviceCreate", status);
        return status;
    }

    PVIRTUAL_KEYBOARD_DEVICE_CONTEXT deviceContext = VirtualKeyboardGetDeviceContext(device);

    // デバイスコンテキストの中身を全部 null にしておく（変なところで Cleanup 走ったら困るので）
    RtlZeroMemory(deviceContext, sizeof(VIRTUAL_KEYBOARD_DEVICE_CONTEXT));

    // リモート HID 作成
    VHF_CONFIG vhfConfig;
    VHF_CONFIG_INIT(&vhfConfig, WdfDeviceWdmGetDeviceObject(device), sizeof(ReportDescriptor), ReportDescriptor);
    vhfConfig.VhfClientContext = deviceContext; // EvtVhfCleanup で使う
    vhfConfig.EvtVhfCleanup = VirtualKeyboardEvtVhfCleanup;

    status = VhfCreate(&vhfConfig, &deviceContext->VhfHandle);

    if (!NT_SUCCESS(status)) {
        TraceErrorStatus("VhfCreate", status);
        return status;
    }

    status = VhfStart(deviceContext->VhfHandle);

    if (!NT_SUCCESS(status)) {
        TraceErrorStatus("VhfStart", status);
        VhfDelete(deviceContext->VhfHandle, FALSE);
        return status;
    }

    // タイマー作成
    status = VirtualKeyboardDeviceCreateKeyDownTimer(device);
    if (NT_SUCCESS(status)) {
        status = VirtualKeyboardDeviceCreateKeyUpTimer(device);
        if (NT_SUCCESS(status)) {
            WdfTimerStart(deviceContext->KeyDownTimer, WDF_REL_TIMEOUT_IN_MS(KEY_DOWN_TIMER_DUE_TIME_MS));
        }
    }

    return STATUS_SUCCESS;
}

VOID VirtualKeyboardDeviceEvtCleanupCallback(_In_ WDFOBJECT Object) {
    PAGED_CODE();

    TraceEnterFunc();

    // デバイスを削除
    PVIRTUAL_KEYBOARD_DEVICE_CONTEXT deviceContext = VirtualKeyboardGetDeviceContext(Object);
    VHFHANDLE hVhf = deviceContext->VhfHandle;
    if (hVhf != NULL) {
        deviceContext->VhfHandle = NULL;
        VhfDelete(hVhf, FALSE);
    }
}

VOID VirtualKeyboardEvtVhfCleanup(_In_ PVOID VhfClientContext) {
    TraceEnterFunc();

    PVIRTUAL_KEYBOARD_DEVICE_CONTEXT deviceContext = (PVIRTUAL_KEYBOARD_DEVICE_CONTEXT)VhfClientContext;
    deviceContext->VhfHandle = NULL;
}

NTSTATUS VirtualKeyboardDeviceCreateKeyDownTimer(WDFDEVICE device) {
    PAGED_CODE();

    WDF_TIMER_CONFIG timerConfig;
    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, VirtualKeyboardDeviceKeyDownTimerEvtTimerFunc, KEY_DOWN_TIMER_INTERVAL_MS);

    WDF_OBJECT_ATTRIBUTES timerAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = device;

    WDFTIMER timer;
    NTSTATUS status = WdfTimerCreate(&timerConfig, &timerAttributes, &timer);

    if (!NT_SUCCESS(status)) {
        TraceErrorStatus("WdfTimerCreate", status);
        return status;
    }

    PVIRTUAL_KEYBOARD_DEVICE_CONTEXT deviceContext = VirtualKeyboardGetDeviceContext(device);
    deviceContext->KeyDownTimer = timer;

    return STATUS_SUCCESS;
}

VOID VirtualKeyboardDeviceKeyDownTimerEvtTimerFunc(_In_	WDFTIMER Timer) {
    TraceEnterFunc();

    PVIRTUAL_KEYBOARD_DEVICE_CONTEXT deviceContext = VirtualKeyboardGetDeviceContext(WdfTimerGetParentObject(Timer));
    VHFHANDLE hVhf = deviceContext->VhfHandle;

    if (hVhf == NULL) return;

    // LSB が A のオンオフを表すので、「1」にする
    UCHAR data = 1;
    HID_XFER_PACKET packet;
    packet.reportBuffer = &data;
    packet.reportBufferLen = 1;
    packet.reportId = 0;

    NTSTATUS status = VhfReadReportSubmit(hVhf, &packet);

    if (NT_SUCCESS(status)) {
        // 時間が経ったらフラグを下す
        WdfTimerStart(deviceContext->KeyUpTimer, WDF_REL_TIMEOUT_IN_MS(KEY_UP_TIMER_DUE_TIME_MS));
    }
    else {
        TraceErrorStatus("VhfReadReportSubmit", status);
    }
}

NTSTATUS VirtualKeyboardDeviceCreateKeyUpTimer(WDFDEVICE device) {
    PAGED_CODE();

    WDF_TIMER_CONFIG timerConfig;
    WDF_TIMER_CONFIG_INIT(&timerConfig, VirtualKeyboardDeviceKeyUpTimerEvtTimerFunc);

    WDF_OBJECT_ATTRIBUTES timerAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = device;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive; // EvtTimerFunc を Passive で実行

    WDFTIMER timer;
    NTSTATUS status = WdfTimerCreate(&timerConfig, &timerAttributes, &timer);

    if (!NT_SUCCESS(status)) {
        TraceErrorStatus("WdfTimerCreate", status);
        return status;
    }

    PVIRTUAL_KEYBOARD_DEVICE_CONTEXT deviceContext = VirtualKeyboardGetDeviceContext(device);
    deviceContext->KeyUpTimer = timer;

    return STATUS_SUCCESS;
}

VOID VirtualKeyboardDeviceKeyUpTimerEvtTimerFunc(_In_ WDFTIMER Timer) {
    PAGED_CODE();

    TraceEnterFunc();

    PVIRTUAL_KEYBOARD_DEVICE_CONTEXT deviceContext = VirtualKeyboardGetDeviceContext(WdfTimerGetParentObject(Timer));
    VHFHANDLE hVhf = deviceContext->VhfHandle;

    if (hVhf == NULL) return;

    UCHAR data = 0;
    HID_XFER_PACKET packet;
    packet.reportBuffer = &data;
    packet.reportBufferLen = 1;
    packet.reportId = 0;

    NTSTATUS status = VhfReadReportSubmit(hVhf, &packet);

    if (!NTSTATUS(status)) {
        TraceErrorStatus("VhfReadReportSubmit", status);
    }

    if (++deviceContext->Counter >= LOOP_COUNT) {
        // これで終わり
        VhfDelete(hVhf, FALSE);
        WdfTimerStop(deviceContext->KeyDownTimer, FALSE);
    }
}
