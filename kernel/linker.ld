ENTRY(kernel_main)

SECTIONS
{
  . = 0x400000;

  __kernel_start = .;

  .text : ALIGN(4096)
  {
    *(.text .text.*)
  }

  .rodata : ALIGN(4096)
  {
    *(.rodata .rodata.*)
  }

  .data : ALIGN(4096)
  {
    *(.data .data.*)
  }

  .bss : ALIGN(4096)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  }

  __kernel_end = .;
}
