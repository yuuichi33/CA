ENTRY(_start)
/* Define a default global pointer for small-data ABI usages */
__global_pointer$ = ORIGIN(ram) + 0x1000;
MEMORY
{
  /* Place program at low addresses so the simulator's Memory backing stays small */
  ram (rwx) : ORIGIN = 0x0, LENGTH = 16M
}
SECTIONS
{
  . = ORIGIN(ram);
  .text : {
    KEEP(*(.init))
    *(.text .text.*)
    *(.rodata .rodata.*)
    KEEP(*(.fini))
    _etext = .;
  } > ram

  .data : {
    _sdata = .;
    *(.data .data.*)
    *(.sdata .sdata.*)
    _edata = .;
  } > ram
  __data_load = _sdata;

  .bss (NOLOAD) : {
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    _ebss = .;
  } > ram

  _stack_top = ORIGIN(ram) + LENGTH(ram);
}
