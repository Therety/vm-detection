#include <stdio.h>
#include "vm_detect.h"

int main() {
    const char* os = detect_os();
    printf("Detected OS: %s\n", os);

    if (is_vm_cpuid()) {
        printf("[!] Hypervisor bit is SET\n");
    }
    if (is_vm_vendor()) {
        printf("[!] Hypervisor vendor string detected\n");
    }
    if (is_vm_rdtsc()) {
        printf("[!] RDTSC timing too fast\n");
    }
    if (is_vm_vmware_io()) {
        printf("[!] Port-based VM check triggered\n");
    }
    if (is_vm_mac()) {
        printf("[!] Virtual MAC address prefix detected\n");
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
