#include <stdio.h>
#include "vm_detect.h"

int main() {
    randomized_delay(); // this can be moved to vm_detect.c when you only need is_vm()
    const char* os = detect_os();
    printf("Detected OS: %s\n", os);

    // this is mainly for debugging, but i'm leaving it in :D
    if (is_vm_cpuid()) {
        printf("[!] Hypervisor bit is SET\n");
    }
    if (is_vm_rdtsc()) {
        printf("[!] RDTSC timing too fast\n");
    }
    if (is_vm_sidt()) {
        printf("[!] IDT in lower memory range\n");
    }
    if (is_vm_sgdt()) {
        printf("[!] GDT in lower memory range\n");
    }
    if (is_vm_cpuid_jitter()) {
        printf("[!] Lower CPU jitter\n");
    }
    if (is_vm_rdtsc_vs_qpc()) {
        printf("[!] Odd rdtsc value\n");
    }
    if (is_vm_throttling()) {
        printf("[!] Slow rdtsc progression\n");
    }
    if (is_vm_clflush_timing()) {
        printf("[!] Too fast cache flush\n");
    }
    if (is_vm_tsc()) {
        printf("[!] Non-invariant tsc\n");
    }


    if (is_vm()) {
        printf("Running in a virtual environment!\n");
    } else {
        printf("Running on physical hardware.\n");
    }

    printf("\nPress Enter to exit...");
    getchar();
    return 0;
}
