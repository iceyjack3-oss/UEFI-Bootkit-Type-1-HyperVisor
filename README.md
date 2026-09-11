# UEFI-Bootkit-Type-1-HyperVisor 
THIS IS NOT TESTED IT MAY CONTAIN ALOT OF BUGS!

# Intel VT-x UEFI Type-1 Hypervisor (Experimental Draft)

⚠️ **PROJECT STATUS: ABANDONED / UNTESTED / INCOMPLETE** ⚠️

This repository contains a raw, low-level structural blueprint for an Intel VT-x Type-1 Hypervisor designed to boot directly from a UEFI environment. 

This project was built as a solo intellectual challenge to map out Ring -1 hardware architectures To bypass Vanguard. Due to the extreme time constraints required to build out a complete nested virtualization engine (to support Windows Hyper-V/VBS), I have decided to archive this codebase and walk away from development.

### 🔴 CRITICAL WARNING
* **This code has NEVER been compiled or booted on physical hardware.**
* It is highly likely to cause an immediate CPU triple fault, machine check exception, or system freeze if executed.
* **DO NOT run this on your main machine.** This is published strictly for educational review and static code analysis.

### 🧩 What is Implemented (Theory Draft)
* **UEFI Boot Entry:** Basic framework initialization.
* **VMX Setup:** Configuration layout for entering VMX operation.
* **Guest/Host State:** Baseline Intel VMCS layout structures.
* **EPT Configuration:** Initial draft for mapping Guest/Host page tables.

### ❌ What is Missing / Broken
* **No Nested Virtualization:** Does not implement the necessary `VMREAD`/`VMWRITE` shadowing traps to support nested hypervisors (like Hyper-V).
* **Untested Register Layouts:** The VMCS bitmasks have not been dynamically verified or debugged.

Feel free to fork the code if you want to use the framework layout for your own low-level research.
