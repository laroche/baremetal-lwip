#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

enum {
 UART_FR_RXFE = 0x10,
 UART0_ADDR = 0x101f1000
};

#define UART_DR(baseaddr) (*(unsigned int *)(baseaddr))
#define UART_FR(baseaddr) (*(((unsigned int *)(baseaddr))+6))

int _close(int file __unused) { return -1; }

int _fstat(int file __unused, struct stat *st) {
 st->st_mode = S_IFCHR;
 return 0;
}

int _isatty(int file __unused) { return 1; }

int _lseek(int file __unused, int ptr __unused, int dir __unused) { return 0; }

int _open(const char *name __unused, int flags __unused, int mode __unused) { return -1; }

int _read(int file __unused, char *ptr, int len) {
 int todo;
 if(len == 0)
  return 0;
 while(UART_FR(UART0_ADDR) & UART_FR_RXFE);
 *ptr++ = UART_DR(UART0_ADDR);
 for(todo = 1; todo < len; todo++) {
  if(UART_FR(UART0_ADDR) & UART_FR_RXFE) {
   break;
  }
  *ptr++ = UART_DR(UART0_ADDR);
 }
 return todo;
}

extern char heap_low; /* Defined by the linker */
extern char heap_top; /* Defined by the linker */
char *heap_end = &heap_low;
caddr_t _sbrk(int incr) {
 char *prev_heap_end = heap_end;

 if (heap_end + incr > &heap_top) {
  /* Heap and stack collision */
  return (caddr_t)0;
 }

 heap_end += incr;
 return (caddr_t) prev_heap_end;
 }

int _write(int file __unused, char *ptr, int len) {
 int todo;

 for (todo = 0; todo < len; todo++) {
  UART_DR(UART0_ADDR) = *ptr++;
 }
 return len;
}

__dead2 void abort (void)
{
  uint32_t addr;

#ifdef __GNUC__
  /* Store instruction causing undefined exception */
  __asm__ __volatile__("sub %0, lr, #4" : "=r" (addr));
#else
  #error "Unsupported compiler."
#endif

  printf("Abort called from instruction address 0x%x.\n", addr);

  _exit(1);
}
