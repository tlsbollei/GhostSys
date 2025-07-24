#include <windows.h>
#include <iostream>

#define IOCTL_RUN_BPF CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main() {
    HANDLE hDevice = CreateFileW(L"\\\\.\\BpfldrPoC",
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open driver\n";
        return 1;
    }

    DWORD returned;
    BOOL res = DeviceIoControl(hDevice, IOCTL_RUN_BPF,
        nullptr, 0,
        nullptr, 0,
        &returned, nullptr);

    if (!res) {
        std::cerr << "Failed to execute BPF syscall program\n";
        CloseHandle(hDevice);
        return 1;
    }
    // by reaching here, we executed the syscall on behalf of the kernel-mode driver, from ring 0, and we have successfuly evaded any user-mode hooks or CET shadow stack protections
    std::cout << "Successfully executed kernel-mode syscall via eBPF JIT\n";

    CloseHandle(hDevice);
    return 0;
}
