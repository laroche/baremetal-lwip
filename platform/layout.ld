ENTRY(_Reset)

SECTIONS
{
  . = 0x10000;
  .text : {
    KEEP(*startup.o (.text*))
    *(.text)
    *(.rodata)
    . = ALIGN(4);
  }
  .data : {
    *(.data)
    . = ALIGN(4);
  }
  __bss_start__ = .;
  .bss (NOLOAD) : {
    *(.bss)
    *(COMMON)
    . = ALIGN(4);
  }
  end = .;
  . = ALIGN(8);
  heap_low = .;		/* for _sbrk */
  . = . + 0x10000;	/* 64kB of heap memory */
  heap_top = .;		/* for _sbrk */
  . = . + 0x10000;	/* 64kB of stack memory */
  stack_top = .;	/* for _Reset in startup.c */
}
