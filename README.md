# VM Detection in C

I've been working on this project for a while, while working on other stuff, and decided to include it here for anybody interested. I am aware there could be issues or false positives depending on the setup — it's meant as a learning tool. Note that there are certain parts written in Assembly (don't get scared, i was learning it on the way :D)

## About

This is a small cross-platform (x86_64 architecture) C project to detect if your program is running in a virtual environment such as VMware, VirtualBox, KVM, Hyper-V or sandbox services like ANY.run.

## How It Works

First, before running any detection checks, the program introduces a randomized delay to allow the execution environment (such as a sandbox or virtual machine) to fully initialize (this helps avoid inaccurate results caused by early execution). 
Then, the program performs several VM detection checks, including:

- CPU feature flags (CPUID hypervisor bit)
- Updated RDTSC timing
- RDTSC :: `QueryPerformanceCounter()`/`clock_gettime()`
- Throttling detection
- Red pill technique*
- Cache flush timing
- CPUID TSC invariance
- ...Working on more...

*IDRT contains address of IDT, by comparing both, program can determine if it's running on a VM or physical.

## Updates
I'll keep updating this repo for aslong i'll be interested in this project. Mostly, i'll be fixing my mistakes made in the past, with some additional changes to make it way better than it originally was.
