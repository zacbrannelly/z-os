ENTRY(kernel_main)

PHDRS
{
  text   PT_LOAD FLAGS(5); /* R + X */
  rodata PT_LOAD FLAGS(4); /* R */
  data   PT_LOAD FLAGS(6); /* R + W */
}

SECTIONS
{
  . = 0xFFFF000000000000;

  __kernel_start = .;

  .text : ALIGN(4096)
  {
    *(.text .text.*)
  } :text

  .rodata : ALIGN(4096)
  {
    *(.rodata .rodata.*)
  } :rodata

  .data : ALIGN(4096)
  {
    *(.data .data.*)
  } :data

  .bss : ALIGN(4096)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } :data

  __kernel_end = .;
}
