SECTIONS
{
 . = 0x10000;
 .ro : {
  *startup.o (.text)
  *(.text)
  *(.rodata)
 }
 . = ALIGN(4);
 .rw : {
  *(.data)
  *(COMMON)
  . = ALIGN(4);
  __bss_start__ = .;
  *(.bss)
 }
 . = ALIGN(4);
 end = .;
 heap_low = .; /* for _sbrk */
 . = . + 0x10000; /* 64kB of heap memory */
 heap_top = .; /* for _sbrk */
 . = . + 0x10000; /* 64kB of stack memory */
 stack_top = .; /* for startup.s */
}
