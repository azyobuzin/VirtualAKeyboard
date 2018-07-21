#pragma once

#define TraceEnterFunc() KdPrint(("Enter %s\n", __FUNCTION__))
#define TraceErrorStatus(funcName, status) KdPrint(("%s returned 0x%x in %s\n", funcName, status, __FUNCTION__))
