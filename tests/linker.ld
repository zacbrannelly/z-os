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

  /* Pin note sections to the text PHDR. Without an explicit :phdr, ld places
     allocated note sections into every PT_LOAD whose flags match, which
     causes the data PT_LOAD to start at the same vaddr as text and overlap. */
  .note.gnu.build-id : ALIGN(4) { *(.note.gnu.build-id) } :text
  .note             : ALIGN(4) { *(.note .note.*)     } :text

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
