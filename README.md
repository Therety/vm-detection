# VM Detection in C

I've been working on this project for a while, while working on other stuff, and decided to include it here for anybody interested. I am aware there could be issues or false positives depending on the setup — it's meant as a learning tool. Note that there are certain parts written in Assembly (don't get scared, i was learning it on the way :D)

## About

This is a small cross-platform (x86_64 architecture) C project to detect if your program is running in a virtual environment (like VMware, VirtualBox, KVM, or Hyper-V).

## How It Works

It uses several checks, including:

- CPU feature flags (CPUID hypervisor bit)
- Updated RDTSC timing
- RDTSC :: QueryPerformanceCounter (Windows)
- Throttling detection
- Red pill technique*
- Cache flush timing
- ...Working on more...

*IDRT contains address of IDT, by comparing both, program can determine if it's running on a VM or physical.

## Updates
I'll keep updating this repo for aslong i'll be interested in this project.
