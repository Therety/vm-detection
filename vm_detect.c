/*

      VM DETECTION IN C
    --------------------
      MADE BY: Therety
      LICENSE: GPL 3.0

*/

#include "vm_detect.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

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

// windows and linux compatibility
#ifdef OS_WINDOWS
    double qpc_now() {
        LARGE_INTEGER freq, counter;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        return (double)counter.QuadPart * 1000.0 / freq.QuadPart;
    }

    void sleep_ms(int ms) {
        Sleep(ms);
    }
#else
    double qpc_now() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e9;
    }

    void sleep_ms(int ms) {
        usleep(ms * 1000);
    }
#endif

void randomized_delay() {
    srand((unsigned int)time(NULL));
    int delay_ms = 3000 + rand() % 2000;
    printf("[?] Delaying startup by %d ms...\n", delay_ms);
    sleep_ms(delay_ms);
}

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

// Updated mean/stddev thresholds: less false positives, more accurate
int is_vm_rdtsc_vs_qpc() {
    const int samples = 10;
    double ratios[samples];
    unsigned int lo1, hi1, lo2, hi2;
    unsigned long long t1, t2;

    for (int i = 0; i < samples; i++) {
        double qpc1 = qpc_now();
        __asm__ volatile ("rdtsc" : "=a"(lo1), "=d"(hi1));
        sleep_ms(10);
        double qpc2 = qpc_now();
        __asm__ volatile ("rdtsc" : "=a"(lo2), "=d"(hi2));

        t1 = ((unsigned long long)hi1 << 32) | lo1;
        t2 = ((unsigned long long)hi2 << 32) | lo2;

        double qpc_diff = qpc2 - qpc1;
        double rdtsc_diff = (double)(t2 - t1);
        ratios[i] = rdtsc_diff / qpc_diff;
    }

    double sum = 0;
    for (int i = 0; i < samples; i++) sum += ratios[i];
    double mean = sum / samples;

    double dev_sum = 0;
    for (int i = 0; i < samples; i++) dev_sum += (ratios[i] - mean) * (ratios[i] - mean);
    double stddev = sqrt(dev_sum / samples);

    //printf("mean=%.2f stddev=%.2f\n", mean, stddev);

    if (mean < 200000.0 || mean > 3000000.0) return 1;
    if (stddev > 2000.0) return 1;
    return 0;
}

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

    return dev < 30;
    //if (dev < 30 && mean < 1000) return 1;
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

// red pill method (updated entropy for sidt)
int is_vm_sidt() {
    unsigned long bases[10];
    int identical = 1;

    for (int i = 0; i < 10; i++) {
        unsigned char idt[6];
        __asm__ volatile ("sidt %0" : "=m"(idt));
        bases[i] = *(unsigned long *)&idt[2];

        if (i > 0 && bases[i] != bases[i - 1])
            identical = 0;
    }

    return identical;
}

int is_vm_sgdt() {
    unsigned char gdt[6];
    __asm__ volatile ("sgdt %0" : "=m"(gdt));
    unsigned long base = *(unsigned long *)&gdt[2];

    return (base < 0x40000000);
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
    int clflush     = is_vm_clflush_timing();
    int tsc         = is_vm_tsc();

    // no negatives for lower-confidence checks,
    // +2 for high confidence checks (more reliable for hardened virt env)
    score += cpuid      ? 1 :  0;
    score += rdtsc      ? 1 :  0;
    score += jitter     ? 1 :  0;
    score += rdtsc_qpc  ? 2 : -1;
    score += throttle   ? 1 :  0;
    score += sidt       ? 1 :  0;
    score += sgdt       ? 1 :  0;
    score += clflush    ? 1 :  0;
    score += tsc        ? 2 : -1;

    // debug
    //printf("Scores: cpuid=%d, rdtsc=%d, jitter=%d, rdtsc_qpc=%d, throttle=%d, sidt=%d, sgdt=%d, clflush=%d, tsc=%d\n", cpuid, rdtsc, jitter, rdtsc_qpc, throttle, sidt, sgdt, clflush, tsc);
    //printf("Final score: %d\n", score);

    return score >= 2;
}
