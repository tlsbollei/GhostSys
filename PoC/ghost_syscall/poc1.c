#include <windows.h>
#include <iostream>

bool get_text_section(uint8_t* base, uint8_t** text_base, size_t* text_size) {
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto section = IMAGE_FIRST_SECTION(nt);

    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(section[i].Name, ".text", 5) == 0) {
            *text_base = base + section[i].VirtualAddress;
            *text_size = section[i].Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

uintptr_t find_syscall_stub(uint32_t ssn) {
    auto ntdll = (uint8_t*)GetModuleHandleW(L"ntdll.dll");
    uint8_t* text_base;
    size_t text_size;
    if (!get_text_section(ntdll, &text_base, &text_size)) {
        std::cerr << "Failed to retrieve the .text section of the PE binary - debug? :(\n";
        return 0;
    }
// find the cet-compliant gadgets in the ntdll.dll .text section
    for (size_t i = 0; i < text_size - 8; ++i) {
        if (text_base[i] == 0xB8 && // mov eax, imm32
            *(uint32_t*)&text_base[i + 1] == ssn &&
            text_base[i + 5] == 0x0F && text_base[i + 6] == 0x05 && // syscall
            text_base[i + 7] == 0xC3) // ret
        {
            return (uintptr_t)&text_base[i];
        }
    }

    std::cerr << "Failed to retrieve CET-Compliant gadget at 0x" << std::hex << ssn << "\n";
    return 0;
}
int main() {
    constexpr uint32_t SSN_NtQuerySystemTim	e = 0x3F; // IRL you would not hardcode this, youd dynamically retrieve it from the stub, but here we simplify for sanity

    uintptr_t stub = find_syscall_stub(SSN_NtQuerySystemTime);
    if (!stub) {
        return 1;
    }

    auto ghost = (NTSTATUS(WINAPI*)(PLARGE_INTEGER))stub;

    LARGE_INTEGER sysTime = {};
    NTSTATUS status = ghost(&sysTime);

    std::cout << "[CET-Ghost] NtQuerySystemTime: " << sysTime.QuadPart
              << " | Status: 0x" << std::hex << status << "\n";

    return 0;
}

/*
call [existing stub in ntdll.dll]

where we called into the function that ended with ret, therefore both the stack and CET shadow stack were left preserved and aligned
shadow stack sees call -> ret, so it is happy
by directly loading the gadgets from the ntdll.dll, where on a CET-Enabled system they are CET-Compliant, we manage to preserve and align the shadow stack
by doing this we managed to successfuly replace the functionality of building inline syscall stubs by staying in accordance with hardware enforced protections
*/
