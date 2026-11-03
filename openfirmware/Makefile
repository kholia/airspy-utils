# OpenHF+ firmware - ATSAM3U1 (Cortex-M3, Thumb-2)
#
# Any arm-none-eabi toolchain works. On Ubuntu/Debian:
#   sudo apt install gcc-arm-none-eabi
# (a local extraction of the .debs works fine without root too)

CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

CFLAGS  := -mcpu=cortex-m3 -mthumb -std=gnu11 -O2 -g3 \
           -Wall -Wextra -ffreestanding -fno-builtin \
           -ffunction-sections -fdata-sections \
           -Iinclude
# fully freestanding link: just our objects plus libgcc (division etc.)
LIBGCC  := $(shell $(CC) -mcpu=cortex-m3 -mthumb -print-libgcc-file-name)
LDFLAGS := -mcpu=cortex-m3 -mthumb -nostdlib \
           -T ld/sam3u1.ld -Wl,--gc-sections -Wl,-Map=build/openhf.map

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))

all: build/openhf.bin build/openhf.elf
	$(SIZE) build/openhf.elf
	@echo "  flash usage above must stay under 65280 bytes so the"
	@echo "  calibration page at 0x8FFC0 stays reserved (linker ASSERTs this)"

build/openhf.elf: $(OBJS) ld/sam3u1.ld
	@mkdir -p build
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LIBGCC) -o $@

build/openhf.bin: build/openhf.elf
	$(OBJCOPY) -O binary $< $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

.PHONY: all clean
