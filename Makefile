# Compile together with FreeRTOS?
FREERTOS = 0

TOOLCHAIN = arm-none-eabi-
COMPILE   = $(TOOLCHAIN)gcc
ARCHIVE   = $(TOOLCHAIN)ar
LINKER    = $(TOOLCHAIN)gcc
OBJCOPY   = $(TOOLCHAIN)objcopy
OBJDUMP   = $(TOOLCHAIN)objdump
SIZE      = $(TOOLCHAIN)size

CFLAGS  = -mcpu=arm926ej-s --specs=nano.specs --specs=nosys.specs -g -O2 -Wall -Wextra -pedantic -Wno-format
#CFLAGS += -Wundef -Wwrite-strings -Wold-style-definition -Wunreachable-code -Waggregate-return -Wlogical-op -Wtrampolines
#CFLAGS += -Wcast-align=strict -Wshadow -Wmissing-prototypes -Wredundant-decls -Wnested-externs -Wcast-qual -Wswitch-default
#CFLAGS += -Wc90-c99-compat -Wc99-c11-compat -Wconversion
LFLAGS = --specs=nano.specs --specs=nosys.specs -nostartfiles -T $(PLATFORM_DIR)/layout.ld -g

#CFLAGS += -ffunction-sections
#LFLAGS += -Wl,--gc-sections
# For debugging:
#LFLAGS += -Wl,--print-gc-sections

QEMU    = qemu-system-arm
QFLAGS  = -M versatilepb -m 128M -nographic
#QNET    = -net nic -net dump,file=vm0.pcap -net tap,ifname=tap0
#QNET    = -net nic -net tap,ifname=tap0
QNET    = -net nic -net user

BIN_DIR      = obj
APP_DIR      = app
PLATFORM_DIR = platform
LINK_TARGET  = $(BIN_DIR)/app.elf
MAPFILE      = $(BIN_DIR)/app.map
LISTFILE     = $(BIN_DIR)/app.lst
BIN_TARGET   = $(BIN_DIR)/app.bin

# names of .c and .s source files in app and platform source directories
APP_SRC = $(wildcard $(APP_DIR)/*.c)
PLATFORM_SRC = $(wildcard $(PLATFORM_DIR)/*.c) 
PLATFORM_ASM = $(wildcard $(PLATFORM_DIR)/*.s) 

#change names of .c and .s files in source dirs to .o files in obj dir
APP_OBJS = $(patsubst $(PLATFORM_DIR)/%.c,$(BIN_DIR)/%.o,$(PLATFORM_SRC)) \
           $(patsubst $(PLATFORM_DIR)/%.s,$(BIN_DIR)/%.o,$(PLATFORM_ASM)) \
           $(patsubst $(APP_DIR)/%.c,$(BIN_DIR)/%.o,$(APP_SRC))

LWIP_LIB  = $(BIN_DIR)/liblwip.a
LWIP_OBJS = $(addprefix $(BIN_DIR)/,\
              init.o def.o dns.o inet_chksum.o ip.o mem.o memp.o netif.o \
              pbuf.o raw.o stats.o sys.o altcp.o altcp_alloc.o altcp_tcp.o \
              tcp.o tcp_in.o tcp_out.o timeouts.o udp.o icmp.o ip4.o \
              ip4_addr.o ip4_frag.o ethernet.o etharp.o acd.o dhcp.o \
              autoip.o sntp.o tcpip.o err.o sockets.o)

ifeq ($(FREERTOS),1)
LWIP_OBJS += $(addprefix $(BIN_DIR)/, sys_arch.o tasks.o list.o queue.o timers.o heap_3.o port.o portISR.o timer.o interrupt.o uart.o)
CFLAGS += -DUSE_LARGE_DEMO -DUSE_NEWLIB
CFLAGS += -DUSE_FREERTOS -DLWIP_PROVIDE_ERRNO -I platform-freertos -I FreeRTOS/include
CFLAGS += -I lwip/contrib/ports/freertos/include -I FreeRTOS/portable/GCC/ARM926EJ-S
vpath %.c lwip/src/api/ lwip/src/core/ lwip/src/netif/ lwip/src/core/ipv4/ lwip/src/apps/sntp/ \
          $(PLATFORM_DIR) $(APP_DIR) platform-freertos/ lwip/contrib/ports/freertos/ FreeRTOS/ FreeRTOS/portable/GCC/ARM926EJ-S/ FreeRTOS/portable/MemMang/
else
vpath %.c lwip/src/api/ lwip/src/core/ lwip/src/netif/ lwip/src/core/ipv4/ lwip/src/apps/sntp/ \
          $(PLATFORM_DIR) $(APP_DIR)
endif

CFLAGS += -I $(PLATFORM_DIR) -I lwip/src/include

# Detect Windows with two possible ways. On Linux start parallel builds:
ifeq ($(OS),Windows_NT)
else
ifeq '$(findstring ;,$(PATH))' ';'
else
CORES?=$(shell (nproc --all || sysctl -n hw.ncpu) 2>/dev/null || echo 1)
ifneq ($(CORES),1)
.PHONY: _all
_all:
	$(MAKE) all -j$(CORES)
endif
endif
endif

.PHONY: all clean run lwip

all : $(BIN_TARGET) # $(LWIP_LIB)

lwip : $(LWIP_LIB)

$(LWIP_LIB) : $(LWIP_OBJS)
	$(ARCHIVE) cr $@ $(LWIP_OBJS)

$(BIN_DIR) :
	mkdir -p $@

$(LWIP_OBJS) $(APP_OBJS): Makefile | $(BIN_DIR)

$(BIN_DIR)/%.o : %.c
	$(COMPILE) $(CFLAGS) -c $< -o $@

# -Wl,--no-warn-rwx-segments
$(LINK_TARGET) : $(LWIP_OBJS) $(APP_OBJS) $(PLATFORM_DIR)/layout.ld
	$(LINKER) $(LFLAGS) $(LWIP_OBJS) $(APP_OBJS) -o $(LINK_TARGET) -Wl,-Map=$(MAPFILE)
	$(OBJDUMP) -d $@ > $(LISTFILE)
	$(SIZE) $@

$(BIN_TARGET) : $(LINK_TARGET)
	$(OBJCOPY) -O binary $(LINK_TARGET) $(BIN_TARGET)

clean : 
	rm -fr $(BIN_DIR)

run : $(BIN_TARGET)
	@echo "Starting qemu, use \"ctrl-a x\" to exit from qemu:"
	QEMU_AUDIO_DRV=none $(QEMU) $(QFLAGS) $(QNET) -kernel $(BIN_TARGET)

