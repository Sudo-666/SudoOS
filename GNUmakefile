# Nuke built-in rules.
.SUFFIXES:

# 定义QEMU模拟器的默认参数，分配2GB内存。
QEMUFLAGS := -m 2G

# 定义操作系统镜像名称
override IMAGE_NAME := sudoOS

# 定义主机编译工具链参数：
HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=


UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)  # macOS
    USER_CC      := x86_64-elf-gcc
	USER_LD      := x86_64-elf-ld
	USER_OBJCOPY := x86_64-elf-objcopy
endif

ifeq ($(UNAME_S),Linux)  # Linux
	USER_CC      := gcc
	USER_LD      := ld
	USER_OBJCOPY := objcopy
endif
# 设置构建目标
.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

# 运行目标：运行ISO镜像（BIOS模式）
.PHONY: run
run: $(IMAGE_NAME).iso
	qemu-system-x86_64 \
		-M q35 \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS)

# 运行ISO镜像（UEFI模式）
.PHONY: run-uefi
run-uefi: edk2-ovmf $(IMAGE_NAME).iso
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS)

# 从HDD镜像运行
.PHONY: run-hdd
run-hdd: $(IMAGE_NAME).hdd
	qemu-system-x86_64 \
		-M q35 \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-hdd-uefi
run-hdd-uefi: edk2-ovmf $(IMAGE_NAME).hdd
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-x86_64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

# =========================================================================
# 依赖构建部分
# =========================================================================

# 下载并解压OVMF固件（UEFI支持）
# 【修复】这里恢复了目标名，并确保反斜杠后没有空格
edk2-ovmf:
	curl -L https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/edk2-ovmf.tar.gz | \
	gunzip | tar -xf -

# 克隆并编译Limine引导加载程序。
boot/limine: 
	@if [ ! -f boot/limine/limine ]; then \
		mkdir -p boot; \
		if [ ! -d boot/limine ]; then \
			git clone https://codeberg.org/Limine/Limine.git boot/limine --branch=v8.x-binary --depth=1; \
		else \
			cd boot/limine && git reset --hard && git clean -fd; \
		fi; \
		$(MAKE) -C boot/limine \
			CC="$(HOST_CC)" \
			CFLAGS="$(HOST_CFLAGS)" \
			CPPFLAGS="$(HOST_CPPFLAGS)" \
			LDFLAGS="$(HOST_LDFLAGS)" \
			LIBS="$(HOST_LIBS)"; \
	else \
		@echo "Limine already compiled, skipping..."; \
	fi

# 运行脚本获取内核依赖。
kernel/.deps-obtained:
	./kernel/get-deps

# 编译内核。
.PHONY: kernel
kernel:  kernel/.deps-obtained
	$(MAKE) -C kernel

# =========================================================================
# 编译用户程序 (完整修复版)
# =========================================================================

# 1. 自动收集 usr/lib 下所有的 .c 文件 (如 stdio.c, syscall.c 等)
USER_LIB_SRC := $(shell find usr/lib -name '*.c')
USER_LIB_OBJ := $(USER_LIB_SRC:.c=.o)

# 2. 编译用户库的通用规则
usr/lib/%.o: usr/lib/%.c
	@mkdir -p $(dir $@)
	$(USER_CC) -c $< -o $@ -ffreestanding -fno-stack-protector -mno-red-zone -Iusr

# 3. 编译 shell.c (之前缺少的规则)
usr/shell.o: usr/shell.c
	$(USER_CC) -c $< -o $@ -ffreestanding -fno-stack-protector -mno-red-zone -Iusr

# 4. 编译 usrmain.o
usr/usrmain.o: usr/usrmain.c
	@mkdir -p usr
	$(USER_CC) -c $< -o $@ -ffreestanding -fno-stack-protector -mno-red-zone -Iusr

# 5. 链接所有文件生成 ELF
# 【修复】链接时包含 shell.o 和所有库文件
# 【修复】不再生成 raw binary，直接输出为 ELF
usr/bin/user.elf: usr/usrmain.o usr/shell.o $(USER_LIB_OBJ)
	@mkdir -p usr/bin
	$(USER_LD) -Ttext 0x1000000 -e usrmain usr/usrmain.o usr/shell.o $(USER_LIB_OBJ) -o $@

# =========================================================================
# 镜像打包部分
# =========================================================================

# ISO镜像构建
$(IMAGE_NAME).iso: boot/limine kernel usr/bin/user.elf limine.conf
	rm -rf iso_root
	mkdir -p iso_root/boot/limine
	mkdir -p iso_root/EFI/BOOT
	
	# 拷贝内核
	cp -v kernel/bin/kernel iso_root/boot/
	
	# 【修复】拷贝用户程序，重命名为 user.bin 以匹配 limine.conf
	cp -v usr/bin/user.elf iso_root/user.bin
	
	# 拷贝配置文件
	cp -v limine.conf iso_root/boot/limine/
	
	# 引导程序文件
	cp -v boot/limine/limine-bios.sys boot/limine/limine-bios-cd.bin boot/limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v boot/limine/BOOTX64.EFI iso_root/EFI/BOOT/
	
	# 打包 ISO
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./boot/limine/limine bios-install $(IMAGE_NAME).iso
	@rm -rf iso_root

# HDD镜像构建
$(IMAGE_NAME).hdd: boot/limine kernel usr/bin/user.elf
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./boot/limine/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin/kernel ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.conf boot/limine/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M boot/limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M boot/limine/BOOTIA32.EFI ::/EFI/BOOT
	# 【修复】拷贝用户程序
	mcopy -i $(IMAGE_NAME).hdd@@1M usr/bin/user.elf ::/user.bin

# 清理目标
.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd 
	rm -f usr/*.o usr/lib/*.o usr/shell.o
	rm -rf usr/bin

# 彻底清理
.PHONY: distclean
distclean: clean
	$(MAKE) -C kernel distclean
	rm -rf kernel-deps boot edk2-ovmf