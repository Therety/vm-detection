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

int is_vm_clflush_timing() {
    volatile int x = 0;
    unsigned long long start, end;
    __asm__ volatile ("mfence\n\t"
                      "rdtsc\n\t"
                      "shl $32, %%rdx\n\t"
                      "or %%rdx, %%rax\n\t"
                      "mov %%rax, %0"
                      : "=r"(start)::"rax", "rdx");

    __asm__ volatile("clflush (%0)" ::"r"(&x));

    __asm__ volatile ("mfence\n\t"
                      "rdtsc\n\t"
                      "shl $32, %%rdx\n\t"
                      "or %%rdx, %%rax\n\t"
                      "mov %%rax, %0"
                      : "=r"(end)::"rax", "rdx");

    return (end - start < 100);
}

// invariance check
int is_vm_tsc() {
    unsigned int eax, edx;
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=d"(edx)
        : "a"(0x80000007)
    );
    return !(edx & (1 << 8));
}

int is_vm() {
    int score = 0;

    int cpuid       = is_vm_cpuid();
    int rdtsc       = is_vm_rdtsc();
    int jitter      = is_vm_cpuid_jitter();
    int rdtsc_qpc   = is_vm_rdtsc_vs_qpc();
    int throttle    = is_vm_throttling();
    int sidt        = is_vm_sidt();
    int sgdt        = is_vm_sgdt();
    int smsw        = is_vm_smsw();
    int clflush     = is_vm_clflush_timing();
    int tsc         = is_vm_tsc();

    // Balanced scoring system
    score += cpuid      ? 1 : -1;
    score += rdtsc      ? 1 : -1;
    score += jitter     ? 1 : -1;
    score += rdtsc_qpc  ? 1 : -1;
    score += throttle   ? 1 : -1;
    score += sidt       ? 1 : -1;
    score += sgdt       ? 1 : -1;
    score += smsw       ? 1 : -1;
    score += clflush    ? 1 : -1;
    score += tsc        ? 1 : -1;

    return score >= 2;
}
