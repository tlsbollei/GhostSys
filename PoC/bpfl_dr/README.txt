Within the kernel_driver.cpp source code, we create a kernel device accessible from user-mode for communication
When user-mode sends the IOCTL_RUN_BPF command, the driver triggers eBPF program load and execution.
The minimal program encodes the instructions mov eax, SSN; syscall; ret.
Once JIT-compiled by ebpfcore.sys, it runs natively inside the kernel at ring 0 and on behalf of kernel-mode, effectively bypassing any user-mode hooks or CET shadow stack protections.
The usermode_loader.c simply opens the device and sends the IOCTL command to trigger the syscall execution.


WARNING!
Real ebpfcore.sys interactions require undocumented APIs, complex program registration, and code injection mechanisms — here we show a high-level PoC skeleton, both for the safety of digital infrastructures and your own mental sanity. Stay legal!
Actual eBPF instructions differ from raw x86 , you would build a valid eBPF program that compiles to your desired kernel instructions.
This assumes the ability to load signed kernel drivers on the target system - similiarly to BYOVD, with the difference that ebpfcore.sys comes pre-shipped.