#ifndef VM_DETECT_H
#define VM_DETECT_H

// Cross-platform OS detection
const char* detect_os();

// Platform-independent VM check
int is_vm();

// Platform-specific checks
int is_vm_cpuid();
int is_vm_vendor();
int is_vm_rdtsc();
int is_vm_vmware_io();
int is_vm_mac();

#endif