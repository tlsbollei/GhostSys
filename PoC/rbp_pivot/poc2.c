#include <windows.h>
#include <iostream>

using pNtQuerySystemTime = NTSTATUS(WINAPI*)(PLARGE_INTEGER);

uint8_t* get_ntdll_base() {
    return reinterpret_cast<uint8_t*>(GetModuleHandleW(L"ntdll.dll"));
}

bool get_text_section(uint8_t* base, uint8_t** text_start, size_t* text_size) {
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            *text_start = base + sec[i].VirtualAddress;
            *text_size = sec[i].Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

uintptr_t find_syscall_stub(uint32_t ssn) {
    uint8_t* base = get_ntdll_base();
    uint8_t* text;
    size_t len;
    if (!get_text_section(base, &text, &len)) return 0;
    for (size_t i = 0; i < len - 8; ++i) {
        if (text[i] == 0xB8 &&
            *reinterpret_cast<uint32_t*>(&text[i + 1]) == ssn &&
            text[i + 5] == 0x0F &&
            text[i + 6] == 0x05 &&
            text[i + 7] == 0xC3) {
            return reinterpret_cast<uintptr_t>(&text[i]);
        }
    }
    return 0;
}

uintptr_t find_push_ret_gadget() {
    uint8_t* base = get_ntdll_base();
    uint8_t* text;
    size_t len;
    if (!get_text_section(base, &text, &len)) return 0;
    for (size_t i = 0; i < len - 5; ++i) {
        if (text[i] == 0x68 &&
            text[i + 5] == 0xC3) {
            return reinterpret_cast<uintptr_t>(&text[i]);
        }
    }
    return 0;
}

int main() {
    constexpr uint32_t SSN = 0x3F;
    uintptr_t syscall_stub = find_syscall_stub(SSN);
    uintptr_t push_ret = find_push_ret_gadget();
    if (!syscall_stub || !push_ret) return -1;

    uint8_t* code = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    code[0] = 0x68;
    *reinterpret_cast<uint32_t*>(&code[1]) = (uint32_t)(syscall_stub);
    code[5] = 0xC3;

    auto trampoline = reinterpret_cast<pNtQuerySystemTime>(code);
    LARGE_INTEGER t;
    NTSTATUS status = trampoline(&t);

    std::cout << "[CET-Ghost-RBP] Time: " << t.QuadPart << " Status: 0x" << std::hex << status << "\n";

    SecureZeroMemory(code, 0x1000);
    VirtualFree(code, 0, MEM_RELEASE);
    return 0;
}

/* eg 

inside of technique 1 we did :

call [existing stub in ntdll.dll]

where we called into the function that ended with ret, therefore both the stack and CET shadow stack were left preserved and aligned
shadow stack sees call -> ret, so it is happy

inside of technique 2 we did :

push <syscall_gadget_addr>
ret

where we pushed the address of the syscall gadget onto the stack and then returned, which also preserved the stack and CET shadow stack alignment. 

both techniques display full CET compliance

*/