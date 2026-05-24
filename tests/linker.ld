ENTRY(_start)

PHDRS
{
  text   PT_LOAD FLAGS(5); /* R + X */
  rodata PT_LOAD FLAGS(4); /* R */
  data   PT_LOAD FLAGS(6); /* R + W */
}

SECTIONS
{
  . = 0x400000;

  .text : ALIGN(4096)
  {
    *(.text .text.*)
  } :text

  .rodata : ALIGN(4096)
  {
    *(.rodata .rodata.*)
  } :rodata

  .tests : ALIGN(8)
  {
    __tests_start = .;
    KEEP(*(.tests))
    __tests_end = .;
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
}
