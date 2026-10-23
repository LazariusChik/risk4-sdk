OUTPUT_ARCH(riscv)
ENTRY(_proto_start)

PHDRS
{
    text    PT_LOAD    FILEHDR PHDRS FLAGS(5);
    interp  PT_INTERP  FLAGS(4);
    data    PT_LOAD    FLAGS(6);
    dynamic PT_DYNAMIC FLAGS(6);
    stack   PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
    . = SIZEOF_HEADERS;
    .interp : { *(.interp) } :text :interp
    .hash : { *(.hash) } :text
    .gnu.hash : { *(.gnu.hash) } :text
    .dynsym : { *(.dynsym) } :text
    .dynstr : { *(.dynstr) } :text
    .gnu.version : { *(.gnu.version) } :text
    .gnu.version_d : { *(.gnu.version_d) } :text
    .gnu.version_r : { *(.gnu.version_r) } :text
    .rela.dyn : { *(.rela.dyn) } :text
    .rela.plt : { *(.rela.plt) } :text
    .plt : { *(.plt) *(.iplt) } :text
    .text : { *(.text .text.*) } :text
    .rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) } :text

    /*
     * ABI 5 flash-XIP layout.
     *
     * The RW segment is placed at a FIXED 0x08000000 virtual bias above the
     * RX segment, matching PROTELF_XIP_FLASH_PSRAM_DELTA in
     * main/kernel/loader/protelf_xip.h. At runtime the kernel maps the xip_apps
     * flash partition read-only and pairs it with a PSRAM arena at exactly
     * that offset, so `data_base - text_base` equals this constant for every
     * slot. proteld_assign_layout() derives its rx_window from this linked
     * delta and refuses to load if the two disagree, which is what keeps the
     * geometry honest without the loader having to trust the manifest.
     *
     * Do not "optimise" this to ALIGN() or LOG2CEIL(): the value is a
     * contract with the arena, not padding. Apps built for the RAM loader
     * use protelf_app_abi4.ld instead.
     */
    . = 0x08000000;
    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    } :data
    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } :data
    .dynamic : { *(.dynamic) } :data :dynamic
    .got : { *(.got .got.*) *(.igot .igot.*) } :data
    .got.plt : { *(.got.plt) *(.igot.plt) } :data
    .data : { *(.data .data.*) } :data
    .sdata : { *(.sdata .sdata.*) } :data
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :data
    .sbss (NOLOAD) : { *(.sbss .sbss.*) } :data

    .riscv.attributes 0 : { *(.riscv.attributes) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.eh_frame_hdr*) }
}
