#include <windows.h>
#include <winternl.h>
#include <iostream>

using pNtQuerySystemTime = NTSTATUS(WINAPI*)(PLARGE_INTEGER);

extern "C" PVOID* NtUserCallOneParam(DWORD, PVOID);

uint8_t* get_ntdll_base() {
    return reinterpret_cast<uint8_t*>(GetModuleHandleW(L"ntdll.dll"));
}

uintptr_t find_syscall_stub(uint32_t ssn) {
    uint8_t* base = get_ntdll_base();
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            uint8_t* text = base + sec[i].VirtualAddress;
            size_t len = sec[i].Misc.VirtualSize;
            for (size_t j = 0; j < len - 8; ++j) {
                if (text[j] == 0xB8 &&
                    *reinterpret_cast<uint32_t*>(&text[j + 1]) == ssn &&
                    text[j + 5] == 0x0F && text[j + 6] == 0x05 &&
                    text[j + 7] == 0xC3) {
                    return reinterpret_cast<uintptr_t>(&text[j]);
                }
            }
        }
    }
    return 0;
}

int main() {
    constexpr uint32_t SSN = 0x3F; // NtQuerySystemTime
    uintptr_t syscall_stub = find_syscall_stub(SSN);
    if (!syscall_stub) {
        std::cerr << "Failed to locate syscall stub\n";
        return -1;
    }

    PVOID* cb_table = *reinterpret_cast<PVOID**>(__readgsqword(0x60) + 0x58);
    PVOID* fnHkINLPCWCHAR = &cb_table[0x11]; // Undocumented entry, unused, purely a placeholder?

    DWORD oldProtect;
    VirtualProtect(fnHkINLPCWCHAR, sizeof(PVOID), PAGE_READWRITE, &oldProtect);
    *fnHkINLPCWCHAR = reinterpret_cast<PVOID>(syscall_stub);
    VirtualProtect(fnHkINLPCWCHAR, sizeof(PVOID), oldProtect, &oldProtect);

    LARGE_INTEGER t = { 0 };
    SendMessageTimeoutW(HWND_BROADCAST, 0, 0, 0, SMTO_NORMAL, 100, (PDWORD_PTR)&t);

    std::cout << "[Syscall-Smuggle-KCT] Time: " << std::hex << t.QuadPart << "\n";

    return 0;
}
