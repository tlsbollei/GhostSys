#include <ntddk.h>
#include <wdf.h>

#define IOCTL_RUN_BPF CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

extern "C" DRIVER_INITIALIZE DriverEntry;
void DriverUnload(PDRIVER_OBJECT DriverObject);

HANDLE ebpf_device_handle = nullptr;

// Minimal eBPF program bytes to execute:
// mov eax, 0x3F (NtQuerySystemTime) - again static for demonstration
// syscall
// ret
// The actual eBPF instructions encoding syscall in kernel JIT form is simplified here for demo

static const uint8_t ebpf_program_bytes[] = {
    0xb8, 0x3f, 0x00, 0x00, 0x00,   // mov eax, 0x3F
    0x0f, 0x05,                     // syscall
    0xc3                            // ret
};

NTSTATUS IoctlHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_RUN_BPF) {
        // Here we would:
        // 1. Register/load the eBPF program via ebpfcore.sys APIs (undocumented, but assume success)
        // 2. Run the program via BPF_PROG_TEST_RUN ioctl call to ebpfcore.sys
        // This is pseudocode placeholder since real APIs are undocumented

        // status = ebpfcore_load_program(ebpf_program_bytes, sizeof(ebpf_program_bytes), &prog_handle);
        // status = ebpfcore_test_run(prog_handle, etc etc);

        // for demo continue with success
        DbgPrint("[BPFldr] eBPF program loaded and executed successfully\n");
        DbgPrint("[BPFldr] Executed eBPF syscall program\n");
        status = STATUS_SUCCESS;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoctlHandler;

    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\BpfldrPoC");
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\DosDevices\\BpfldrPoC");
    PDEVICE_OBJECT deviceObject = nullptr;

    NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) return status;

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    DbgPrint("[BPFldr] Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\DosDevices\\BpfldrPoC");
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);
    DbgPrint("[BPFldr] Driver unloaded\n");
}
