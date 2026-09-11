#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Protocol/MpService.h>
#include <intrin.h>

#define IA32_FEATURE_CONTROL_MSR 0x3A
#define IA32_VMX_BASIC_MSR 0x480
#define IA32_VMX_PINBASED_CTLS_MSR 0x481
#define IA32_VMX_PROCBASED_CTLS_MSR 0x482
#define IA32_VMX_EXIT_CTLS_MSR 0x483
#define IA32_VMX_ENTRY_CTLS_MSR 0x484
#define IA32_VMX_TRUE_PINBASED_MSR 0x48D
#define IA32_VMX_TRUE_PROCBASED_MSR 0x48E
#define IA32_VMX_TRUE_EXIT_MSR 0x48F
#define IA32_VMX_TRUE_ENTRY_MSR 0x490
#define IA32_EFER_MSR 0xC0000080
#define IA32_FS_BASE_MSR 0xC0000100
#define IA32_GS_BASE_MSR 0xC0000101

#define COM1_PORT 0x3F8
#define MAX_CPUS 64

#define EPT_R (1ULL << 0)
#define EPT_W (1ULL << 1)
#define EPT_X (1ULL << 2)
#define EPT_PS (1ULL << 7)
#define EPT_MT_WB 6
#define EPT_4K_PA_MASK 0x000FFFFFFFFFF000ULL
#define EPT_2M_PA_MASK 0x000FFFFFFFE00000ULL

#define VM_EXIT_REASON 0x00004402
#define EXIT_QUALIFICATION 0x00006400
#define VM_INSTRUCTION_ERROR 0x00004400
#define VM_EXIT_INSTR_LENGTH 0x0000440C
#define VM_EXIT_INSTR_INFO 0x0000440E

#define GUEST_CR0 0x6800
#define GUEST_CR4 0x6804
#define GUEST_CR3 0x6802
#define GUEST_DR7 0x681A
#define GUEST_RFLAGS 0x6820
#define GUEST_RSP 0x681C
#define GUEST_RIP 0x681E

#define GUEST_GDTR_BASE 0x6816
#define GUEST_GDTR_LIMIT 0x4810
#define GUEST_IDTR_BASE 0x6818
#define GUEST_IDTR_LIMIT 0x4812

#define GUEST_CS_BASE 0x6808
#define GUEST_CS_LIMIT 0x4802
#define GUEST_CS_ACCESS_RIGHTS 0x4816
#define GUEST_CS_SELECTOR 0x0802

#define GUEST_SS_BASE 0x680A
#define GUEST_SS_LIMIT 0x4804
#define GUEST_SS_ACCESS_RIGHTS 0x4818
#define GUEST_SS_SELECTOR 0x0804

#define GUEST_ES_BASE 0x6806
#define GUEST_ES_LIMIT 0x4800
#define GUEST_ES_ACCESS_RIGHTS 0x4814
#define GUEST_ES_SELECTOR 0x0800

#define GUEST_DS_BASE 0x680C
#define GUEST_DS_LIMIT 0x4806
#define GUEST_DS_ACCESS_RIGHTS 0x481A
#define GUEST_DS_SELECTOR 0x0806

#define GUEST_GS_BASE 0x6810
#define GUEST_GS_SELECTOR 0x080A
#define GUEST_GS_LIMIT 0x480A
#define GUEST_GS_ACCESS_RIGHTS 0x481E

#define GUEST_FS_BASE 0x680E
#define GUEST_FS_SELECTOR 0x0808
#define GUEST_FS_LIMIT 0x4808
#define GUEST_FS_ACCESS_RIGHTS 0x481C

#define GUEST_TR_SELECTOR 0x0000080E
#define GUEST_TR_LIMIT 0x0000480E
#define GUEST_TR_BASE 0x00006814
#define GUEST_TR_ACCESS_RIGHTS 0x00004822

#define HOST_CR0 0x00006C00
#define HOST_CR3 0x00006C02
#define HOST_CR4 0x00006C04
#define HOST_CS_SELECTOR 0x00000C02
#define HOST_SS_SELECTOR 0x00000C04
#define HOST_DS_SELECTOR 0x00000C06
#define HOST_FS_BASE 0x00006C06
#define HOST_GS_BASE 0x00006C08
#define HOST_TR_BASE 0x00006C0A
#define HOST_GDTR_BASE 0x00006C0C
#define HOST_IDTR_BASE 0x00006C0E
#define HOST_TR_SELECTOR 0x00000C0C
#define HOST_RSP 0x00006C14
#define HOST_RIP 0x00006C16

#define VMCS_CTRL_PIN_BASED 0x4000
#define VMCS_CTRL_PROC_BASED 0x4002
#define VMCS_CTRL_EXIT 0x400C
#define VMCS_CTRL_ENTRY 0x4012
#define VMCS_CTRL_MSR_BITMAP 0x2004
#define VMCS_CTRL_EPT_POINTER 0x201A
#define VMCS_GUEST_IA32_EFER 0x2806
#define VMCS_HOST_IA32_EFER 0x2C02

#define EXIT_REASON_CPUID 10
#define EXIT_REASON_VMCALL 18
#define EXIT_REASON_VMCLEAR 19
#define EXIT_REASON_VMLAUNCH 20
#define EXIT_REASON_VMPTRLD 21
#define EXIT_REASON_VMPTRST 22
#define EXIT_REASON_VMREAD 23
#define EXIT_REASON_VMRESUME 24
#define EXIT_REASON_VMWRITE 25
#define EXIT_REASON_VMXOFF 26
#define EXIT_REASON_VMXON 27
#define EXIT_REASON_CR_ACCESS 28
#define EXIT_REASON_MSR_READ 31
#define EXIT_REASON_MSR_WRITE 32
#define EXIT_REASON_EPT_VIOLATION 48

typedef struct _GUEST_REGS
{
    UINT64 rax, rbx, rcx, rdx;
    UINT64 rsi, rdi, rbp;
    UINT64 r8, r9, r10, r11;
    UINT64 r12, r13, r14, r15;
} GUEST_REGS, *PGUEST_REGS;

#pragma pack(push, 1)
typedef struct _VMX_SEGMENT_ACCESS_RIGHTS
{
    UINT32 Type : 4;
    UINT32 S : 1;
    UINT32 DPL : 2;
    UINT32 P : 1;
    UINT32 Reserved1 : 4;
    UINT32 AVL : 1;
    UINT32 L : 1;
    UINT32 DB : 1;
    UINT32 G : 1;
    UINT32 Unusable : 1;
    UINT32 Reserved2 : 15;
} VMX_SEGMENT_ACCESS_RIGHTS;

typedef struct _GDTR
{
    UINT16 Limit;
    UINT64 Base;
} GDTR, *PGDTR;

typedef struct _GDT_ENTRY_64
{
    UINT16 LimitLow;
    UINT16 BaseLow;
    UINT8 BaseMiddle;
    UINT8 AccessRights;
    UINT8 FlagsLimitHigh;
    UINT8 BaseHigh;
    UINT32 BaseUpper;
    UINT32 Reserved;
} GDT_ENTRY_64, *PGDT_ENTRY_64;
#pragma pack(pop)

GUEST_REGS gExitRegs = {0};
UINT64 gL1VmxonPa = 0;
UINT64 gL1VmcsPa = 0;

STATIC UINT8 gExitStack[8192] __declspec(align(16));
STATIC UINT8 gGuestStack[8192] __declspec(align(16));

STATIC UINT64 *gEptPml4 = NULL;
STATIC UINT64 *gEptPdpt = NULL;
STATIC UINT64 *gEptPd[512] = {0};

STATIC UINT64 *gGuestPml4 = NULL;
STATIC UINT64 *gGuestPdpt = NULL;
STATIC UINT64 *gGuestPd[512] = {0};

STATIC EFI_MP_SERVICES_PROTOCOL *gMpServices = NULL;

UINT64 read_rsp(VOID);
VOID store_gdtr(VOID *destination);
VOID store_tr_selector(UINT16 *selector);
UINT16 read_cs(VOID);
UINT16 read_ss(VOID);
UINT16 read_ds(VOID);
UINT16 read_es(VOID);
VOID invept_all_context(VOID);
VOID AsmVmExitEntry(VOID);
VOID AsmGuestEntry(VOID);
VOID asmregsguest(VOID);

VOID EFIAPI init_serial(VOID)
{
    IoWrite8(COM1_PORT + 1, 0x00);
    IoWrite8(COM1_PORT + 3, 0x80);
    IoWrite8(COM1_PORT + 0, 0x03);
    IoWrite8(COM1_PORT + 1, 0x00);
    IoWrite8(COM1_PORT + 3, 0x03);
}

VOID EFIAPI print_serial_char(IN CHAR8 c)
{
    while ((IoRead8(COM1_PORT + 5) & 0x20) == 0) { }
    IoWrite8(COM1_PORT, (UINT8)c);
}

VOID EFIAPI print_serial(IN CONST CHAR8 *str)
{
    if (str == NULL) return;
    while (*str != '\0') {
        if (*str == '\n') print_serial_char('\r');
        print_serial_char(*str++);
    }
}

STATIC VOID hv_fail_fast(IN CONST CHAR8 *Message)
{
    print_serial(Message);
    while (1) { CpuDeadLoop(); }
}

STATIC VOID *allocate(VOID)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS Address;

    Status = gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, &Address);
    if (EFI_ERROR(Status)) {
        CHAR8 Buf[128];
        AsciiSPrint(Buf, sizeof(Buf), "AllocatePages failed: %r\n", Status);
        hv_fail_fast(Buf);
    }
    ZeroMem((VOID *)(UINTN)Address, 4096);
    return (VOID *)(UINTN)Address;
}

VOID handleLaunch(VOID)
{
    UINT64 instr_error = 0;
    CHAR8 Buf[128];

    __vmx_vmread(VM_INSTRUCTION_ERROR, &instr_error);
    AsciiSPrint(Buf, sizeof(Buf),
                "VMLAUNCH FAILED: VM_INSTRUCTION_ERROR=0x%lx\n", instr_error);
    hv_fail_fast(Buf);
}

VOID HvVmExitHandler(VOID)
{
    UINT64 reason64 = 0;
    UINT64 qual = 0;
    UINT64 insn_len = 0;
    UINT64 rip = 0;
    UINT32 reason;
    CHAR8 Buf[192];

    __vmx_vmread(VM_EXIT_REASON, &reason64);
    reason = (UINT32)(reason64 & 0xFFFF);
    __vmx_vmread(EXIT_QUALIFICATION, &qual);
    __vmx_vmread(VM_EXIT_INSTR_LENGTH, &insn_len);
    __vmx_vmread(GUEST_RIP, &rip);

    AsciiSPrint(Buf, sizeof(Buf),
                "[L0] exit=%u qual=0x%lx rip=0x%lx len=%lu\n",
                reason, qual, rip, insn_len);
    print_serial(Buf);

    switch (reason) {
    case EXIT_REASON_VMXON:
        print_serial("[L0] VMXON intercepted -> success\n");
        gExitRegs.rax = 0;
        break;

    case EXIT_REASON_VMPTRLD:
        print_serial("[L0] VMPTRLD intercepted -> success\n");
        gExitRegs.rax = 0;
        break;

    case EXIT_REASON_VMWRITE:
        AsciiSPrint(Buf, sizeof(Buf),
                    "[L0] VMWRITE field=0x%lx value=0x%lx\n",
                    gExitRegs.rcx, gExitRegs.rdx);
        print_serial(Buf);
        break;

    case EXIT_REASON_VMREAD:
        AsciiSPrint(Buf, sizeof(Buf),
                    "[L0] VMREAD field=0x%lx -> return 0x80000001\n",
                    gExitRegs.rcx);
        print_serial(Buf);
        gExitRegs.rdx = 0x80000001ULL;
        break;

    case EXIT_REASON_VMXOFF:
        print_serial("[L0] VMXOFF intercepted\n");
        break;

    case EXIT_REASON_VMCALL:
        print_serial("[L0] VMCALL from L1 -- Milestone 1 complete\n");
        while (1) { CpuDeadLoop(); }
        break;

    default:
        AsciiSPrint(Buf, sizeof(Buf),
                    "[L0] unhandled exit reason=%u\n", reason);
        print_serial(Buf);
        while (1) { CpuDeadLoop(); }
    }

    __vmx_vmwrite(GUEST_RIP, rip + insn_len);
}

BOOLEAN isVmxReady(VOID)
{
    UINT64 msrv = AsmReadMsr64(IA32_FEATURE_CONTROL_MSR);
    if ((msrv & 1) == 0) { print_serial("VMX not locked\n"); return FALSE; }
    if ((msrv & 4) == 0) { print_serial("VMX disabled in BIOS\n"); return FALSE; }
    print_serial("VMX enabled and ready\n");
    return TRUE;
}

STATIC VOID setup_control_fields(VOID)
{
    UINT64 basic = AsmReadMsr64(IA32_VMX_BASIC_MSR);
    BOOLEAN use_true = (basic & (1ULL << 55)) != 0;

    UINT64 pin_msr = use_true ? AsmReadMsr64(IA32_VMX_TRUE_PINBASED_MSR)
                                : AsmReadMsr64(IA32_VMX_PINBASED_CTLS_MSR);
    UINT64 proc_msr = use_true ? AsmReadMsr64(IA32_VMX_TRUE_PROCBASED_MSR)
                                : AsmReadMsr64(IA32_VMX_PROCBASED_CTLS_MSR);
    UINT64 exit_msr = use_true ? AsmReadMsr64(IA32_VMX_TRUE_EXIT_MSR)
                                : AsmReadMsr64(IA32_VMX_EXIT_CTLS_MSR);
    UINT64 entry_msr = use_true ? AsmReadMsr64(IA32_VMX_TRUE_ENTRY_MSR)
                                : AsmReadMsr64(IA32_VMX_ENTRY_CTLS_MSR);

    UINT32 pin_ctls = ((UINT32)(pin_msr >> 32)) & (UINT32)pin_msr;
    UINT32 proc_ctls = ((UINT32)(proc_msr >> 32)) & (UINT32)proc_msr;
    UINT32 exit_ctls = ((UINT32)(exit_msr >> 32)) & (UINT32)exit_msr;

    UINT32 desired_entry = (1U << 9) | (1U << 15);
    UINT32 entry_ctls = ((UINT32)(entry_msr >> 32) | desired_entry)
                        & (UINT32)entry_msr;

    proc_ctls |= (1U << 17);

    __vmx_vmwrite(VMCS_CTRL_PIN_BASED, pin_ctls);
    __vmx_vmwrite(VMCS_CTRL_PROC_BASED, proc_ctls);
    __vmx_vmwrite(VMCS_CTRL_EXIT, exit_ctls);
    __vmx_vmwrite(VMCS_CTRL_ENTRY, entry_ctls);
}

STATIC VOID setup_host_state(VOID)
{
    UINT8 gdtr_buf[10];
    UINT8 idtr_buf[10];
    UINT16 tr_sel = 0;
    UINT64 tr_base;
    PGDT_ENTRY_64 tr_desc;
    GDTR gdtr = {0};

    __vmx_vmwrite(HOST_CR0, AsmReadCr0());
    __vmx_vmwrite(HOST_CR3, AsmReadCr3());
    __vmx_vmwrite(HOST_CR4, AsmReadCr4());

    __vmx_vmwrite(HOST_CS_SELECTOR, read_cs());
    __vmx_vmwrite(HOST_SS_SELECTOR, read_ss());
    __vmx_vmwrite(HOST_DS_SELECTOR, read_ds());

    store_tr_selector(&tr_sel);
    __vmx_vmwrite(HOST_TR_SELECTOR, tr_sel);

    __vmx_vmwrite(HOST_FS_BASE, AsmReadMsr64(IA32_FS_BASE_MSR));
    __vmx_vmwrite(HOST_GS_BASE, AsmReadMsr64(IA32_GS_BASE_MSR));

    store_gdtr(gdtr_buf);
    gdtr.Limit = *(UINT16 *)&gdtr_buf[0];
    gdtr.Base = *(UINT64 *)&gdtr_buf[2];

    tr_desc = (PGDT_ENTRY_64)(gdtr.Base + (tr_sel & ~0x7));
    tr_base = ((UINT64)tr_desc->BaseLow)
            | ((UINT64)tr_desc->BaseMiddle << 16)
            | ((UINT64)tr_desc->BaseHigh << 24)
            | ((UINT64)tr_desc->BaseUpper << 32);

    __vmx_vmwrite(HOST_TR_BASE, tr_base);
    __vmx_vmwrite(HOST_GDTR_BASE, gdtr.Base);

    __sidt(idtr_buf);
    __vmx_vmwrite(HOST_IDTR_BASE, *(UINT64 *)&idtr_buf[2]);

    __vmx_vmwrite(HOST_RSP, (UINT64)(UINTN)(gExitStack + sizeof(gExitStack) - 16));
    __vmx_vmwrite(HOST_RIP, (UINT64)(UINTN)AsmVmExitEntry);
    __vmx_vmwrite(VMCS_HOST_IA32_EFER, AsmReadMsr64(IA32_EFER_MSR));
}

STATIC UINT64 get_max_physical_address(VOID)
{
    EFI_STATUS Status;
    UINTN MapSize = 0;
    EFI_MEMORY_DESCRIPTOR *Map = NULL;
    UINTN MapKey, DescSize;
    UINT32 DescVer;
    UINT64 MaxPhys = 0x100000000ULL;

    Status = gBS->GetMemoryMap(&MapSize, Map, &MapKey, &DescSize, &DescVer);
    if (Status != EFI_BUFFER_TOO_SMALL || MapSize == 0) return MaxPhys;

    MapSize += 2 * DescSize;
    Map = AllocatePool(MapSize);
    if (Map == NULL) return MaxPhys;

    Status = gBS->GetMemoryMap(&MapSize, Map, &MapKey, &DescSize, &DescVer);
    if (EFI_ERROR(Status)) { FreePool(Map); return MaxPhys; }

    MaxPhys = 0;
    {
        UINT8 *Cursor = (UINT8 *)Map;
        UINTN i;
        for (i = 0; i < MapSize / DescSize; i++) {
            EFI_MEMORY_DESCRIPTOR *D = (EFI_MEMORY_DESCRIPTOR *)Cursor;
            UINT64 End = D->PhysicalStart + ((UINT64)D->NumberOfPages << 12);
            if (End > MaxPhys) MaxPhys = End;
            Cursor += DescSize;
        }
    }
    FreePool(Map);
    return MaxPhys;
}

STATIC VOID ept_build_2mb_identity(IN UINT64 MaxPhys)
{
    UINT64 GbCount, g, i;

    gEptPml4 = allocate();
    gEptPdpt = allocate();
    gEptPml4[0] = (UINT64)(UINTN)gEptPdpt | EPT_R | EPT_W | EPT_X;

    GbCount = (MaxPhys + 0x3FFFFFFFULL) >> 30;
    if (GbCount == 0) GbCount = 1;
    if (GbCount > 512) GbCount = 512;

    for (g = 0; g < GbCount; g++) {
        UINT64 *pd = allocate();
        gEptPd[g] = pd;
        gEptPdpt[g] = (UINT64)(UINTN)pd | EPT_R | EPT_W | EPT_X;
        for (i = 0; i < 512; i++) {
            pd[i] = (g * 0x40000000ULL + i * 0x200000ULL)
                    | EPT_R | EPT_W | EPT_X | EPT_PS;
        }
    }
}

STATIC BOOLEAN ept_split_to_4k(IN UINT64 TargetPhys, IN UINT64 TargetPerms)
{
    UINT64 Gb = TargetPhys >> 30;
    UINT64 PdIdx = (TargetPhys >> 21) & 0x1FF;
    UINT64 PtIdx = (TargetPhys >> 12) & 0x1FF;
    UINT64 *pd;
    UINT64 pde;
    UINT64 *pt;
    UINT64 Base2M;
    UINT64 Perms;
    UINT64 i;

    if (Gb >= 512 || gEptPd[Gb] == NULL) return FALSE;

    pd = gEptPd[Gb];
    pde = pd[PdIdx];

    if (pde & EPT_PS) {
        pt = allocate();
        Base2M = pde & EPT_2M_PA_MASK;
        Perms = pde & (EPT_R | EPT_W | EPT_X);
        for (i = 0; i < 512; i++) {
            pt[i] = (Base2M + i * 0x1000ULL) | Perms;
        }
        pd[PdIdx] = (UINT64)(UINTN)pt | Perms;
    }

    pt = (UINT64 *)(UINTN)(pd[PdIdx] & EPT_4K_PA_MASK);
    pt[PtIdx] = (TargetPhys & EPT_4K_PA_MASK) | TargetPerms;
    return TRUE;
}

STATIC VOID ept_hide_page(IN UINT64 TargetPhys)
{
    if (!ept_split_to_4k(TargetPhys, EPT_R | EPT_W))
        hv_fail_fast("ept_hide_page failed\n");
    invept_all_context();
}

STATIC VOID ept_install_into_vmcs(VOID)
{
    UINT64 Eptp = (UINT64)(UINTN)gEptPml4 | EPT_MT_WB | (3ULL << 3);
    __vmx_vmwrite(VMCS_CTRL_EPT_POINTER, Eptp);
}

STATIC VOID build_guest_page_tables(IN UINT64 MaxPhys)
{
    UINT64 GbCount, g, i;

    gGuestPml4 = allocate();
    gGuestPdpt = allocate();
    gGuestPml4[0] = (UINT64)(UINTN)gGuestPdpt | 0x3;

    GbCount = (MaxPhys + 0x3FFFFFFFULL) >> 30;
    if (GbCount == 0) GbCount = 1;
    if (GbCount > 512) GbCount = 512;

    for (g = 0; g < GbCount; g++) {
        UINT64 *pd = allocate();
        gGuestPd[g] = pd;
        gGuestPdpt[g] = (UINT64)(UINTN)pd | 0x3;
        for (i = 0; i < 512; i++) {
            pd[i] = (g * 0x40000000ULL + i * 0x200000ULL) | 0x83;
        }
    }
}

VOID setvmcsguest(PGUEST_REGS Regs)
{
    UINT64 cr0, cr4, rflags;
    UINT64 cr0f0, cr0f1, cr4f0, cr4f1;
    UINT8 gdtr_buf[10];
    UINT8 idtr_buf[10];
    UINT16 tr_selector = 0;
    UINT64 tr_base;
    UINT32 tr_limit;
    PGDT_ENTRY_64 tr_desc;
    GDTR gdtr = {0};
    VMX_SEGMENT_ACCESS_RIGHTS csac;
    VMX_SEGMENT_ACCESS_RIGHTS ssac;
    VMX_SEGMENT_ACCESS_RIGHTS unusable;
    VMX_SEGMENT_ACCESS_RIGHTS trac;
    UINT32 ssrights;

    UNREFERENCED_PARAMETER(Regs);

    cr0 = AsmReadCr0();
    cr4 = AsmReadCr4();
    rflags = AsmReadEflags();

    cr0f0 = AsmReadMsr64(0x486);
    cr0f1 = AsmReadMsr64(0x487);
    cr0 = (cr0 | cr0f0) & cr0f1;
    __vmx_vmwrite(GUEST_CR0, cr0);

    cr4f0 = AsmReadMsr64(0x488);
    cr4f1 = AsmReadMsr64(0x489);
    cr4 = (cr4 | cr4f0) & cr4f1;
    __vmx_vmwrite(GUEST_CR4, cr4);

    __vmx_vmwrite(GUEST_CR3, (UINT64)(UINTN)gGuestPml4);
    __vmx_vmwrite(GUEST_DR7, AsmReadDr7());

    rflags &= ~0xFFFFFFFFFFC08028ULL;
    rflags |= (1ULL << 1);
    __vmx_vmwrite(GUEST_RFLAGS, rflags);

    __vmx_vmwrite(GUEST_RSP, (UINT64)(UINTN)(gGuestStack + sizeof(gGuestStack) - 16));
    __vmx_vmwrite(GUEST_RIP, (UINT64)(UINTN)AsmGuestEntry);

    store_gdtr(gdtr_buf);
    __vmx_vmwrite(GUEST_GDTR_BASE, *(UINT64 *)&gdtr_buf[2]);
    __vmx_vmwrite(GUEST_GDTR_LIMIT, *(UINT16 *)&gdtr_buf[0]);

    __sidt(idtr_buf);
    __vmx_vmwrite(GUEST_IDTR_BASE, *(UINT64 *)&idtr_buf[2]);
    __vmx_vmwrite(GUEST_IDTR_LIMIT, *(UINT16 *)&idtr_buf[0]);

    __vmx_vmwrite(GUEST_CS_SELECTOR, read_cs());
    __vmx_vmwrite(GUEST_CS_BASE, 0x0);
    __vmx_vmwrite(GUEST_CS_LIMIT, 0xFFFFFFFF);
    ZeroMem(&csac, sizeof(csac));
    csac.Type = 0x09; csac.L = 1; csac.P = 1; csac.S = 1; csac.G = 1;
    __vmx_vmwrite(GUEST_CS_ACCESS_RIGHTS, *(UINT32 *)&csac);

    __vmx_vmwrite(GUEST_SS_SELECTOR, read_ss());
    __vmx_vmwrite(GUEST_SS_BASE, 0x0);
    __vmx_vmwrite(GUEST_SS_LIMIT, 0xFFFFFFFF);
    ZeroMem(&ssac, sizeof(ssac));
    ssac.Type = 0x03; ssac.P = 1; ssac.S = 1; ssac.G = 1;
    ssrights = *(UINT32 *)&ssac;
    __vmx_vmwrite(GUEST_SS_ACCESS_RIGHTS, ssrights);

    __vmx_vmwrite(GUEST_DS_SELECTOR, read_ds());
    __vmx_vmwrite(GUEST_DS_BASE, 0x0);
    __vmx_vmwrite(GUEST_DS_LIMIT, 0xFFFFFFFF);
    __vmx_vmwrite(GUEST_DS_ACCESS_RIGHTS, ssrights);

    __vmx_vmwrite(GUEST_ES_SELECTOR, read_es());
    __vmx_vmwrite(GUEST_ES_BASE, 0x0);
    __vmx_vmwrite(GUEST_ES_LIMIT, 0xFFFFFFFF);
    __vmx_vmwrite(GUEST_ES_ACCESS_RIGHTS, ssrights);

    ZeroMem(&unusable, sizeof(unusable));
    unusable.Unusable = 1;

    __vmx_vmwrite(GUEST_GS_SELECTOR, 0);
    __vmx_vmwrite(GUEST_GS_LIMIT, 0xFFFFFFFFULL);
    __vmx_vmwrite(GUEST_GS_ACCESS_RIGHTS, *(UINT32 *)&unusable);
    __vmx_vmwrite(GUEST_GS_BASE, AsmReadMsr64(IA32_GS_BASE_MSR));

    __vmx_vmwrite(GUEST_FS_SELECTOR, 0);
    __vmx_vmwrite(GUEST_FS_LIMIT, 0xFFFFFFFFULL);
    __vmx_vmwrite(GUEST_FS_ACCESS_RIGHTS, *(UINT32 *)&unusable);
    __vmx_vmwrite(GUEST_FS_BASE, AsmReadMsr64(IA32_FS_BASE_MSR));

    store_tr_selector(&tr_selector);
    __vmx_vmwrite(GUEST_TR_SELECTOR, tr_selector);

    store_gdtr(&gdtr);
    tr_desc = (PGDT_ENTRY_64)(gdtr.Base + (tr_selector & ~0x7));
    tr_base = ((UINT64)tr_desc->BaseLow)
            | ((UINT64)tr_desc->BaseMiddle << 16)
            | ((UINT64)tr_desc->BaseHigh << 24)
            | ((UINT64)tr_desc->BaseUpper << 32);
    tr_limit = tr_desc->LimitLow | ((tr_desc->FlagsLimitHigh & 0x0F) << 16);
    __vmx_vmwrite(GUEST_TR_BASE, tr_base);
    __vmx_vmwrite(GUEST_TR_LIMIT, tr_limit);

    ZeroMem(&trac, sizeof(trac));
    trac.Type = 0xB; trac.P = 1;
    __vmx_vmwrite(GUEST_TR_ACCESS_RIGHTS, *(UINT32 *)&trac);

    __vmx_vmwrite(VMCS_GUEST_IA32_EFER,
                  (1ULL << 0) | (1ULL << 8) | (1ULL << 10));
}

VOID HyperVisorintel(IN UINTN CpuIndex)
{
    UINT64 vmx_basic;
    UINT32 vmcs_revid;
    UINT64 *vmxon;
    UINT64 vmxon_pa;
    UINT64 *vmcs;
    UINT64 vmcs_pa;
    UINT8 s;
    UINT64 *l1Vmxon;
    UINT64 *l1Vmcs;
    UINT8 launch_status;

    UNREFERENCED_PARAMETER(CpuIndex);

    if (!isVmxReady()) return;

    {
        UINT64 cr4 = AsmReadCr4();
        cr4 |= (1ULL << 13);
        AsmWriteCr4(cr4);
    }

    vmx_basic = AsmReadMsr64(IA32_VMX_BASIC_MSR);
    vmcs_revid = (UINT32)(vmx_basic & 0x7FFFFFFF);

    vmxon = allocate();
    *(UINT32 *)vmxon = vmcs_revid;
    vmxon_pa = (UINT64)(UINTN)vmxon;

    s = __vmx_on(&vmxon_pa);
    if (s != 0) {
        CHAR8 Buf[128];
        AsciiSPrint(Buf, sizeof(Buf), "VMXON failed: 0x%x\n", (UINT32)s);
        hv_fail_fast(Buf);
    }
    print_serial("VMXON OK\n");

    vmcs = allocate();
    *(UINT32 *)vmcs = vmcs_revid;
    vmcs_pa = (UINT64)(UINTN)vmcs;

    s = __vmx_vmclear(&vmcs_pa);
    if (s != 0) {
        CHAR8 Buf[128];
        AsciiSPrint(Buf, sizeof(Buf), "VMCLEAR failed: 0x%x\n", (UINT32)s);
        hv_fail_fast(Buf);
    }

    s = __vmx_vmptrld(&vmcs_pa);
    if (s != 0) {
        CHAR8 Buf[128];
        AsciiSPrint(Buf, sizeof(Buf), "VMPTRLD failed: 0x%x\n", (UINT32)s);
        hv_fail_fast(Buf);
    }
    print_serial("VMPTRLD OK\n");

    setup_control_fields();
    setup_host_state();
    setvmcsguest(NULL);
    ept_install_into_vmcs();

    l1Vmxon = allocate();
    l1Vmcs = allocate();
    *(UINT32 *)l1Vmxon = vmcs_revid;
    *(UINT32 *)l1Vmcs = vmcs_revid;
    gL1VmxonPa = (UINT64)(UINTN)l1Vmxon;
    gL1VmcsPa = (UINT64)(UINTN)l1Vmcs;

    print_serial("Entering L1 stub...\n");

    launch_status = __vmx_vmlaunch();
    if (launch_status != 0) handleLaunch();
}

STATIC VOID EFIAPI LaunchyperVcore(IN VOID *ProcedureArgument)
{
    UNREFERENCED_PARAMETER(ProcedureArgument);
    HyperVisorintel(0);
}

EFI_STATUS EFIAPI UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    UINT64 MaxPhys;

    UNREFERENCED_PARAMETER(ImageHandle);
    UNREFERENCED_PARAMETER(SystemTable);

    init_serial();

    {
        int cpuinfoi[4] = {0};
        __cpuid(cpuinfoi, 1);
        if ((cpuinfoi[2] & (1 << 5)) == 0) {
            print_serial("Intel VMX not supported\n");
            return EFI_UNSUPPORTED;
        }
    }

    Status = gBS->LocateProtocol(&gEfiMpServiceProtocolGuid, NULL, (VOID **)&gMpServices);
    if (EFI_ERROR(Status)) hv_fail_fast("LocateProtocol(MpService) failed\n");

    MaxPhys = get_max_physical_address();
    ept_build_2mb_identity(MaxPhys);
    build_guest_page_tables(MaxPhys);

    LaunchyperVcore(NULL);

    return EFI_SUCCESS;
}