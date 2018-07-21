#pragma once

extern "C" {
	NTSTATUS VirtualKeyboardCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);
}
