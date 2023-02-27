SECTIONS
{
 . = 0x10000;
 .ro : {
  *startup.o (.text)
  *(.text)
  *(.rodata)
 }
 .rw : {
  *(.data)
  __bss_start__ = .;
  *(.bss)
  __bss_end__ = .;
  *(COMMON)
 }
 . = ALIGN(8);
 end = .;
 heap_low = .; /* for _sbrk */
 . = . + 0x10000; /* 64kB of heap memory */
 heap_top = .; /* for _sbrk */
 . = . + 0x10000; /* 64kB of stack memory */
 stack_top = .; /* for startup.s */
}
