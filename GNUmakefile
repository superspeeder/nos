CC = x86_64-elf-gcc
LD = x86_64-elf-ld
NASM = nasm

NASM_SOURCES := $(shell find src/ -name *.nasm)
C_SOURCES := $(shell find src/ -name *.c)
C_HEADERS := $(shell find src/ -name *.h)
C_OBJECTS := $(patsubst src/%.c,build/%.o,$(C_SOURCES))
NASM_OBJECTS := $(patsubst src/%.nasm,build/%.nasm.o,$(NASM_SOURCES))

QEMU_FLAGS := -m 8G -serial stdio
CFLAGS := -ffreestanding -nostdlib -Isrc/ -std=gnu23 -mno-sse -mno-avx -msoft-float
LDFLAGS := -nostdlib

.PHONY: all
all: build/os.iso

$(NASM_OBJECTS): build/%.nasm.o: src/%.nasm
	mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

$(C_OBJECTS): build/%.o: src/%.c $(C_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.elf: $(C_OBJECTS) $(NASM_OBJECTS) linker.ld
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -Tlinker.ld $(C_OBJECTS) $(NASM_OBJECTS) -o $@


build/os.iso: build/kernel.elf grub.cfg
	mkdir -p build/isodir/boot/grub
	cp build/kernel.elf build/isodir/boot/kernel.elf
	cp grub.cfg build/isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/isodir

.PHONY: run
run: build/os.iso
	qemu-system-x86_64 $(QEMU_FLAGS) -cdrom build/os.iso

.PHONY: clean
clean:
	rm -rf build



