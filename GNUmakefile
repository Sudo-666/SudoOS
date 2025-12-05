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

# 设置构建目标
# 构建ISO镜像文件
.PHONY: all
all: $(IMAGE_NAME).iso

# 构建HDD硬盘镜像
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

# 依赖构建部分

# # 下载并解压OVMF固件（UEFI支持）
# edk2-ovmf:
# 	curl -L https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/edk2-ovmf.tar.gz | gunzip | tar -xf -

# 克隆并编译Limine引导加载程序。
boot/limine: 
	@if [ ! -f boot/limine/limine ]; then \
		mkdir -p boot; \
		if [ ! -d boot/limine ]; then \
			git clone https://codeberg.org/Limine/Limine.git boot/limine --branch=v10.x-binary --depth=1; \
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

# ISO镜像构建
$(IMAGE_NAME).iso: boot/limine kernel
	rm -rf iso_root
	mkdir -p iso_root/boot
	cp -v kernel/bin/kernel iso_root/boot/
	mkdir -p iso_root/boot/limine
	cp -v limine.conf boot/limine/limine-bios.sys boot/limine/limine-bios-cd.bin boot/limine/limine-uefi-cd.bin iso_root/boot/limine/
	mkdir -p iso_root/EFI/BOOT
	cp -v boot/limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v boot/limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./boot/limine/limine bios-install $(IMAGE_NAME).iso
	rm -rf iso_root

# HDD镜像构建
$(IMAGE_NAME).hdd: boot/limine kernel
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

# 清理目标
.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd

# 彻底清理，包括下载的依赖。
.PHONY: distclean
distclean: clean
	$(MAKE) -C kernel distclean
	rm -rf kernel-deps boot edk2-ovmf
