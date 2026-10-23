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

    /* ABI 4 RAM-loader layout. */
    . = MAX(0x4000, 1 << LOG2CEIL(.));
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
