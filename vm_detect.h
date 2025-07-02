#ifndef VM_DETECT_H
#define VM_DETECT_H

// Cross-platform OS detection
const char* detect_os();

// Platform-independent VM check
int is_vm();

// Detection checks
int is_vm_cpuid();
int is_vm_rdtsc();
int is_vm_sidt();
int is_vm_sgdt();
int is_vm_smsw();
int is_vm_cpuid_jitter();
int is_vm_rdtsc_vs_qpc();
int is_vm_throttling();
int is_vm_clflush_timing();
int is_vm_tsc();

#endif
