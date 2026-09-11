BITS 64
DEFAULT REL
SECTION .text

extern setvmcsguest
extern handleLaunch
extern HvVmExitHandler
extern gExitRegs

global asmregsguest
global AsmVmExitEntry

asmregsguest:
    push    r15
    push    r14
    push    r13
    push    r12
    push    r11
    push    r10
    push    r9
    push    r8
    push    rbp
    push    rdi
    push    rsi
    push    rdx
    push    rcx
    push    rbx
    push    rax

    mov     rcx, rsp

    sub     rsp, 32
    call    setvmcsguest
    add     rsp, 32

    pop     rax
    pop     rbx
    pop     rcx
    pop     rdx
    pop     rsi
    pop     rdi
    pop     rbp
    pop     r8
    pop     r9
    pop     r10
    pop     r11
    pop     r12
    pop     r13
    pop     r14
    pop     r15

    vmresume
    jz      .try_launch
    ud2

.try_launch:
    vmlaunch
    jz      .both_failed
    ud2

.both_failed:
    sub     rsp, 32
    call    handleLaunch
    add     rsp, 32
    vmxoff
    ret

AsmVmExitEntry:
    push    r15
    push    r14
    push    r13
    push    r12
    push    r11
    push    r10
    push    r9
    push    r8
    push    rbp
    push    rdi
    push    rsi
    push    rdx
    push    rcx
    push    rbx
    push    rax

    mov     rax, rsp
    lea     rcx, [rel gExitRegs]
    mov     edx, 15
.copy_loop:
    mov     r8, [rax]
    mov     [rcx], r8
    add     rax, 8
    add     rcx, 8
    dec     edx
    jnz     .copy_loop

    sub     rsp, 32
    call    HvVmExitHandler
    add     rsp, 32

    lea     rcx, [rel gExitRegs]
    mov     rax, [rcx +   0]
    mov     rbx, [rcx +   8]
    mov     rdx, [rcx +  24]
    mov     rsi, [rcx +  32]
    mov     rdi, [rcx +  40]
    mov     rbp, [rcx +  48]
    mov     r8,  [rcx +  56]
    mov     r9,  [rcx +  64]
    mov     r10, [rcx +  72]
    mov     r11, [rcx +  80]
    mov     r12, [rcx +  88]
    mov     r13, [rcx +  96]
    mov     r14, [rcx + 104]
    mov     r15, [rcx + 112]
    mov     rcx, [rcx +  16]

    vmresume
    ud2