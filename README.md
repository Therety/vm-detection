# VM Detection in C

I've been working on this project for a while, while working on other stuff, and decided to include it here for anybody interested. I am aware there could be issues or false positives depending on the setup — it's meant as a learning tool. Note that there are some parts written in Assembly (don't get scared, i was learning it on the way :D)

## About

This is a small cross-platform (x86_64 architecture) C project to detect if your program is running in a virtual machine (like VMware, VirtualBox, KVM, or Hyper-V).

## How It Works

It uses several checks, including:

- CPU feature flags (CPUID hypervisor bit)
- Virtual machine vendor strings (like "KVMKVMKVM" or "VMwareVMware")
- RDTSC timing (VMs are often slower)
- VMware I/O port trick (on Linux)
- MAC address vendor prefixes
