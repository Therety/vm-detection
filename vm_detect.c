#include "vm_detect.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
    #include <windows.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
#else
    #define OS_LINUX
    #include <stdint.h>
    #include <dirent.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

const char* detect_os() {
#ifdef OS_WINDOWS
    return "Windows";
#elif defined(OS_LINUX)
    return "Linux";
#else
    return "Unknown";
#endif
}

int is_vm_cpuid() {
    unsigned int eax = 1, ebx = 0, ecx = 0, edx = 0;

    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(eax)
    );

    return (ecx >> 31) & 1;  // return bit 31 of ECX (hypervisor present)
}

int is_vm_vendor() {
    unsigned int eax = 0x40000000;
    unsigned int ebx, ecx, edx;
    char vendor[13];

    __asm__ volatile (
        "cpuid"
        : "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(eax)
    );

    memcpy(&vendor[0], &ebx, 4);
    memcpy(&vendor[4], &ecx, 4);
    memcpy(&vendor[8], &edx, 4);
    vendor[12] = '\0';

    const char* known_vendors[] = {
        "KVMKVMKVM", "Microsoft Hv", "VMwareVMware", "XenVMMXenVMM"
    };

    for (int i = 0; i < 4; i++) {
        if (strcmp(vendor, known_vendors[i]) == 0)
            return 1;
    }
    return 0;
}

int is_vm_rdtsc() {
    unsigned int lo1, hi1, lo2, hi2;
    unsigned long long t1, t2;

    __asm__ volatile ("rdtsc" : "=a"(lo1), "=d"(hi1));
    for (volatile int i = 0; i < 1000000; i++);
    __asm__ volatile ("rdtsc" : "=a"(lo2), "=d"(hi2));

    t1 = ((unsigned long long)hi1 << 32) | lo1;
    t2 = ((unsigned long long)hi2 << 32) | lo2;

    return (t2 - t1 < 500);
}

#ifdef OS_WINDOWS
int is_vm_rdtsc_vs_qpc() {
    LARGE_INTEGER qpc1, qpc2;
    unsigned int lo1, hi1, lo2, hi2;
    unsigned long long t1, t2;

    QueryPerformanceCounter(&qpc1);
    __asm__ volatile ("rdtsc" : "=a"(lo1), "=d"(hi1));
    Sleep(10);
    QueryPerformanceCounter(&qpc2);
    __asm__ volatile ("rdtsc" : "=a"(lo2), "=d"(hi2));

    t1 = ((unsigned long long)hi1 << 32) | lo1;
    t2 = ((unsigned long long)hi2 << 32) | lo2;

    double qpc_diff = (double)(qpc2.QuadPart - qpc1.QuadPart);
    unsigned long long rdtsc_diff = t2 - t1;
    double ratio = rdtsc_diff / qpc_diff;

    return (ratio < 500 || ratio > 5000);
}
#else
int is_vm_rdtsc_vs_qpc() { return 0; }
#endif

int is_vm_cpuid_jitter() {
    unsigned int lo1, hi1, lo2, hi2;
    unsigned long long diffs[10];

    for (int i = 0; i < 10; i++) {
        __asm__ volatile ("rdtsc" : "=a"(lo1), "=d"(hi1));
        __asm__ volatile ("cpuid" : : "a"(0));
        __asm__ volatile ("rdtsc" : "=a"(lo2), "=d"(hi2));

        diffs[i] = (((unsigned long long)hi2 << 32) | lo2) -
                   (((unsigned long long)hi1 << 32) | lo1);
    }

    double mean = 0;
    for (int i = 0; i < 10; i++) mean += diffs[i];
    mean /= 10;

    double dev = 0;
    for (int i = 0; i < 10; i++) dev += (diffs[i] - mean) * (diffs[i] - mean);
    dev = sqrt(dev / 10);

    return dev < 50;
}

int is_vm_throttling() {
    unsigned int lo1, hi1, lo2, hi2;
    unsigned long long t1, t2;

#ifdef OS_WINDOWS
    Sleep(5);
#else
    usleep(5000);
#endif

    __asm__ volatile ("rdtsc" : "=a"(lo1), "=d"(hi1));
#ifdef OS_WINDOWS
    Sleep(5);
#else
    usleep(5000);
#endif
    __asm__ volatile ("rdtsc" : "=a"(lo2), "=d"(hi2));

    t1 = ((unsigned long long)hi1 << 32) | lo1;
    t2 = ((unsigned long long)hi2 << 32) | lo2;

    return (t2 - t1 < 500000);
}

int is_vm_vmware_io() {
#ifdef OS_WINDOWS
    return 0; // I/O port access blocked on user-mode Windows
#else
    int detected = 0;
    __asm__ volatile (
        "mov $0x564d5868, %%eax;"
        "mov $0, %%ebx;"
        "mov $0xa, %%ecx;"
        "mov $0x5658, %%dx;"
        "in %%eax, %%dx;"
        "cmp $0x564d5868, %%ebx;"
        "sete %0;"
        : "=r" (detected)
        :
        : "eax", "ebx", "ecx", "edx"
    );
    return detected;
#endif
}

int is_vm_mac() {
    const char* vm_prefixes[] = {
        "00:05:69", "00:0C:29", "00:1C:14", "00:50:56", "08:00:27" // MAC identifiers
    };

#ifdef OS_WINDOWS
    IP_ADAPTER_INFO adapter[10];
    DWORD buflen = sizeof(adapter);
    if (GetAdaptersInfo(adapter, &buflen) != ERROR_SUCCESS) return 0;

    PIP_ADAPTER_INFO p = adapter;
    char mac_str[18];

    while (p) {
        snprintf(mac_str, sizeof(mac_str),
            "%02X:%02X:%02X", p->Address[0], p->Address[1], p->Address[2]);

        for (int i = 0; i < 5; i++) {
            if (strncmp(mac_str, vm_prefixes[i], strlen(vm_prefixes[i])) == 0)
                return 1;
        }
        p = p->Next;
    }
    return 0;
#else
    DIR* dir = opendir("/sys/class/net");
    if (!dir) return 0;

    struct dirent* entry;
    char path[256], mac[32];
    int fd;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "/sys/class/net/%s/address", entry->d_name);
        fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        if (read(fd, mac, sizeof(mac)) > 0) {
            mac[17] = '\0';
            for (int i = 0; i < 5; i++) {
                if (strncmp(mac, vm_prefixes[i], strlen(vm_prefixes[i])) == 0) {
                    close(fd);
                    closedir(dir);
                    return 1;
                }
            }
        }
        close(fd);
    }
    closedir(dir);
    return 0;
#endif
}

// red pill method
int is_vm_sidt() {
    unsigned char idt[6];
    __asm__ volatile ("sidt %0" : "=m"(idt));
    unsigned long base = *(unsigned long *)&idt[2];

    return (base < 0xd0000000); // xd
}

int is_vm_sgdt() {
    unsigned char gdt[6];
    __asm__ volatile ("sgdt %0" : "=m"(gdt));
    unsigned long base = *(unsigned long *)&gdt[2];

    return (base < 0xd0000000);
}

// checks reserved bit
int is_vm_smsw() {
    unsigned long msw = 0;
    __asm__ volatile (
        "smsw %0"
        : "=r"(msw)
    );
    return (msw >> 0x1f) != 0;
}

int is_vm() {
    int score = 0;

    int cpuid       = is_vm_cpuid();
    int vendor      = is_vm_vendor();
    int rdtsc       = is_vm_rdtsc();
    int jitter      = is_vm_cpuid_jitter();
    int rdtsc_qpc   = is_vm_rdtsc_vs_qpc();
    int throttle    = is_vm_throttling();
    int mac         = is_vm_mac();
    int sidt        = is_vm_sidt();
    int sgdt        = is_vm_sgdt();
    int smsw        = is_vm_smsw();
    int vmware_io   = is_vm_vmware_io();

    // Balanced scoring system
    score += cpuid      ? 1 : -1;
    score += vendor     ? 1 : -1;
    score += rdtsc      ? 1 : -1;
    score += jitter     ? 1 : -1;
    score += rdtsc_qpc  ? 1 : -1;
    score += throttle   ? 1 : -1;
    score += mac        ? 1 : -1;
    score += sidt       ? 1 : -1;
    score += sgdt       ? 1 : -1;
    score += smsw       ? 1 : -1;
    score += vmware_io  ? 1 : -1;

    // allow Hyper-V on physical machine (this often triggers false positive)
    if (cpuid && vendor) {
        char vendor_buf[13];
        unsigned int eax = 0x40000000, ebx, ecx, edx;

        __asm__ volatile (
            "cpuid"
            : "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(eax)
        );

        memcpy(&vendor_buf[0], &ebx, 4);
        memcpy(&vendor_buf[4], &ecx, 4);
        memcpy(&vendor_buf[8], &edx, 4);
        vendor_buf[12] = '\0';

        if (strcmp(vendor_buf, "Microsoft Hv") == 0 && score <= 2) {
            return 0;
        }
    }

    return score >= 2;
}
