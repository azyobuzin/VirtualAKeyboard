#pragma once

#define TraceEnterFunc() KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "VirtualKeyboard: Enter %s\n", __FUNCTION__))
#define TraceErrorStatus(funcName, status) KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "VirtualKeyboard: %s returned 0x%x in %s\n", funcName, status, __FUNCTION__))
