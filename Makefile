TOOLCHAIN = arm-none-eabi
COMPILE   = $(TOOLCHAIN)-gcc
ASSEMBLE  = $(TOOLCHAIN)-as
ARCHIVE   = $(TOOLCHAIN)-ar
LINKER    = $(TOOLCHAIN)-gcc
OBJCOPY   = $(TOOLCHAIN)-objcopy

CFLAGS  = -mcpu=arm926ej-s --specs=nano.specs --specs=nosys.specs -O2 -Wall -Wextra -pedantic -Wno-format -I $(PLATFORM_DIR)
#CFLAGS += -Wundef -Wwrite-strings -Wold-style-definition -Wunreachable-code -Waggregate-return -Wlogical-op -Wtrampolines
#CFLAGS += -Wcast-align=strict -Wshadow -Wmissing-prototypes -Wredundant-decls -Wnested-externs -Wcast-qual -Wswitch-default
#CFLAGS += -Wc90-c99-compat -Wc99-c11-compat -Wconversion
ASFLAGS = -mcpu=arm926ej-s

QEMU    = qemu-system-arm
QFLAGS  = -M versatilepb -m 128M -nographic
#QNET    = -net nic -net dump,file=vm0.pcap -net tap,ifname=tap0
QNET    = -net nic -net tap,ifname=tap0

BIN_DIR      = ./obj
APP_DIR      = ./app
PLATFORM_DIR = ./platform
LDSCRIPT     = $(PLATFORM_DIR)/layout.ld
LINK_TARGET  = $(BIN_DIR)/app.elf
MAPFILE      = $(BIN_DIR)/app.map
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
              ip4_addr.o ip4_frag.o ethernet.o etharp.o)

LWIP_INCS = -I lwip/src -I lwip/src/include/ -I lwip/src/api/\
            -I lwip/src/core -I lwip/src/netif -I lwip/src/core/ipv4\
            -I lwip/src/include/lwip
vpath %.c lwip/src/api/ lwip/src/core/ lwip/src/netif/ lwip/src/core/ipv4/\
          $(PLATFORM_DIR) $(APP_DIR)

.PHONY: all clean run lwip

all : | $(LWIP_LIB) $(BIN_TARGET)

lwip : $(LWIP_LIB)

$(LWIP_LIB) : $(LWIP_OBJS)
	$(ARCHIVE) crv $@ $(LWIP_OBJS)

$(BIN_DIR)/%.o : %.c
	$(COMPILE) -g -o $@ -c $(CFLAGS) $(LWIP_INCS) $<

$(BIN_DIR)/%.o : $(PLATFORM_DIR)/%.s
	$(ASSEMBLE) -g -o $@ -c $(ASFLAGS) $<

# -Wl,--no-warn-rwx-segments
$(LINK_TARGET) : $(APP_OBJS)
	$(LINKER) --specs=nano.specs --specs=nosys.specs -T $(LDSCRIPT) \
    $(LWIP_OBJS) $(APP_OBJS) -o $(LINK_TARGET) -Wl,-Map=$(MAPFILE)

$(BIN_TARGET) : $(LINK_TARGET)
	$(OBJCOPY) -O binary $(LINK_TARGET) $(BIN_TARGET)

clean : 
	rm -f $(BIN_DIR)/*

run : $(BIN_TARGET)
	@echo "Starting qemu, use \"ctrl-a x\" to exit from qemu:"
	$(QEMU) $(QFLAGS) $(QNET) -kernel $(BIN_TARGET)

