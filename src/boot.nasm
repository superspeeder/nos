section .multiboot
MBMAGIC equ 0xE85250D6
MBHDLEN equ mbend - mbstart
MBARCH  equ 0
MBCHKSM equ -(MBMAGIC + MBHDLEN + MBARCH)

MB_TAG_FB   equ 5
MB_TAG_NULL equ 0

align 4
mbstart:
.header:
    dd MBMAGIC
    dd MBARCH
    dd MBHDLEN
    dd MBCHKSM

.tag_fb:
    dw MB_TAG_FB
    dw 0            ; flags
    dd .tag_null - .tag_fb
    dd 800
    dd 600
    dd 0

.tag_null:
    dd 0
    dd 0
mbend:

section .bss
stack_bottom:
resb 16384
stack_top:
global mbi_data
mbi_data: resb 8


section .text

CR0_PAGING equ 1 << 31
PML4T_ADDR equ 0x1000
PDPT_ADDR equ 0x2000
PDT_ADDR equ 0x3000
SIZEOF_PAGE_TABLE equ 4096
PT_ADDR_MASK equ 0xffffffffff000
PT_PRESENT equ 1
PT_READABLE equ 2
PT_HUGE equ 1 << 7
ENTRIES_PER_PT equ 512
SIZEOF_PT_ENTRY equ 8
PAGE_SIZE_HUGE equ 0x200000
CR4_PAE_ENABLE equ 1 << 5
EFER_MSR equ 0xC0000080
EFER_LM_ENABLE equ 1 << 8
CR0_PM_ENABLE equ 1 << 0
CR0_PG_ENABLE equ 1 << 31

[bits 32]
global _start:function (_start.end - _start)
_start:
    mov esp, stack_top

    ; store mbi pointer.
    mov DWORD [mbi_data], ebx
    mov DWORD [mbi_data + 4], 0

.enterLongMode:
    ; disable 32-bit paging
    mov eax, cr0
    and eax, ~CR0_PAGING
    mov cr0, eax

    mov edi, PML4T_ADDR
    mov cr3, edi

    ; zero out the page table memory (I think I'm encountering errors from uninitialized memory).
    xor eax, eax

    mov ecx, SIZEOF_PAGE_TABLE >> 2
    rep stosd

    mov ecx, SIZEOF_PAGE_TABLE >> 2
    mov edi, PDPT_ADDR
    rep stosd

    mov edi, cr3
    mov DWORD [edi], PDPT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_READABLE

    mov edi, PDPT_ADDR
    mov DWORD [edi], PDT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_READABLE

    mov edi, PDT_ADDR
    mov ebx, PT_PRESENT | PT_READABLE | PT_HUGE
    mov ecx, ENTRIES_PER_PT
.ptSetEntry:
    mov DWORD [edi], ebx
    add ebx, PAGE_SIZE_HUGE
    add edi, SIZEOF_PT_ENTRY
    loop .ptSetEntry

    mov eax, cr4
    or eax, CR4_PAE_ENABLE
    mov cr4, eax
    
    ; set msr
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LM_ENABLE
    wrmsr

    ; jmp $
    ; enable pm and CR0_PAGING
    mov eax, cr0
    or eax, CR0_PG_ENABLE | CR0_PM_ENABLE
    mov cr0, eax

    lgdt [gdtr64]
    jmp KMCODE_SEG:callMain
.end:

[bits 64]
callMain:
    cli
    mov ax, KMDATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    extern kernel_main
    call kernel_main

    jmp $


section .data
gdt_64:
.null:
    dq 0
.kmcode:
    ; base = 0 ; limit = 0xfffff ; access = 0x9A ; flags = 0xA
    dw 0xFF
    dw 0x00
    db 0x0
    db 0x9A
    db 10101111b
    db 0x0

.kmdata:
    ; base = 0 ; limit = 0xfffff ; access = 0x92 ; flags = 0xC
    dw 0xFF
    dw 0x00
    db 0x0
    db 0x92
    db 11001111b
    db 0x0

.umdata:
    ; base = 0 ; limit = 0xfffff ; access = 0xF2 ; flags = 0xC
    dw 0xFF
    dw 0x00
    db 0x0
    db 0xF2
    db 11001111b
    db 0x0

.umcode:
    ; base = 0 ; limit = 0xfffff ; access = 0xFA ; flags = 0xA
    dw 0xFF
    dw 0x00
    db 0x0
    db 0xFA
    db 10101111b
    db 0x0
.end:

GDT_64_LEN equ gdt_64.end - gdt_64
KMCODE_SEG equ gdt_64.kmcode - gdt_64
KMDATA_SEG equ gdt_64.kmdata - gdt_64
UMDATA_SEG equ gdt_64.umdata - gdt_64
UMCODE_SEG equ gdt_64.umcode - gdt_64

gdtr64:
    dw GDT_64_LEN - 1
    dq gdt_64
