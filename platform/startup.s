.global _Reset
_Reset:
  /* set stack pointer */
  ldr sp, =stack_top

  /* clear bss */
  ldr r0, =__bss_start__
  ldr r1, =end
  mov r2, #0
bss_clear_loop:
  cmp r0, r1
  strlo r2, [r0], #4
  blo bss_clear_loop

  /* main() */
  bl main

  /* endless loop */
  b .

