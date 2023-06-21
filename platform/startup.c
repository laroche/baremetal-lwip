__attribute__ ((naked,used)) void _Reset (void)
{
	/* setup stack pointer */
	__asm__ __volatile__("ldr sp, =stack_top");

	/* clear BSS */
        __asm__ __volatile__(
		"ldr r0, =__bss_start__\n"
		"ldr r1, =end\n"
		"mov r2, #0\n"
	"bss_clear_loop:\n"
		"cmp r0, r1\n"
		"strlo r2, [r0], #4\n"
		"blo bss_clear_loop\n");

	/* main() */
	__asm__ __volatile__("bl main");

	/* endless loop */
	__asm__ __volatile__("b .");
}
