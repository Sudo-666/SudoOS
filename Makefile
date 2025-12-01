KERNEL = kernel.elf
ISO = SudoOS.iso

CC = x86_64-linux-gnu-g++
LD = x86_64-linux-gnu-ld

# 编译参数
CFLAGS = -g -O2 -pipe -Wall -Wextra -ffreestanding -fno-rtti -fno-exceptions \
         -m64 -march=x86-64 -mno-red-zone -mcmodel=kernel -fno-pic -fno-pie \
         -I src -I src/kernel -I src/user -I src/lib

LDFLAGS = -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 -T src/linker.ld

# 扫描源文件 (包括 .cpp 和 .S)
SRCS = $(wildcard src/kernel/*.cpp) \
       $(wildcard src/kernel/arch/*.cpp) \
       $(wildcard src/kernel/arch/*.S) \
       $(wildcard src/kernel/drivers/*.cpp) \
       $(wildcard src/user/*.cpp)

# 分别处理 .cpp -> .o 和 .S -> .o
OBJS_CPP = $(patsubst src/%.cpp, build/%.o, $(filter %.cpp, $(SRCS)))
OBJS_ASM = $(patsubst src/%.S, build/%.o, $(filter %.S, $(SRCS)))
OBJS = $(OBJS_CPP) $(OBJS_ASM)

all: $(ISO)

# 规则：编译 C++
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 规则：编译汇编 (.S)
build/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接 (注意依赖 src/linker.ld)
$(KERNEL): $(OBJS) src/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 打包 ISO
$(ISO): $(KERNEL) limine.conf
	@mkdir -p dist/iso_root
	cp $(KERNEL) limine.conf dist/iso_root/
	cp external/limine/limine-bios.sys external/limine/limine-bios-cd.bin external/limine/limine-uefi-cd.bin dist/iso_root/
	mkdir -p dist/iso_root/EFI/BOOT
	cp external/limine/limine-uefi-cd.bin dist/iso_root/EFI/BOOT/BOOTX64.EFI
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label \
		dist/iso_root -o $(ISO)
	@echo "Build Success!"

run: $(ISO)
	qemu-system-x86_64 -M q35 -m 2G -cdrom $(ISO) -boot d -vga std

clean:
	rm -rf build dist $(KERNEL) $(ISO)