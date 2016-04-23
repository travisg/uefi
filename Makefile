
ARCH		:= x86_64

EFI_TOOLCHAIN	:=
EFI_CC		:= $(EFI_TOOLCHAIN)gcc
EFI_LD		:= $(EFI_TOOLCHAIN)ld
EFI_OBJCOPY	:= $(EFI_TOOLCHAIN)objcopy

EFI_PATH	:= gnu-efi
EFI_LIB_PATHS	:= $(EFI_PATH)/$(ARCH)/lib $(EFI_PATH)/$(ARCH)/gnuefi
EFI_INC_PATHS	:= $(EFI_PATH)/inc $(EFI_PATH)/inc/$(ARCH) $(EFI_PATH)/inc/protocol

EFI_CRT0	:= $(EFI_PATH)/$(ARCH)/gnuefi/crt0-efi-$(ARCH).o
EFI_LINKSCRIPT	:= $(EFI_PATH)/gnuefi/elf_$(ARCH)_efi.lds

EFI_CFLAGS	:= -fpic -fshort-wchar -fno-stack-protector -mno-red-zone
EFI_CFLAGS	+= -Wall
EFI_CFLAGS	+= -ffreestanding -nostdinc -Iinc
EFI_CFLAGS	+= $(patsubst %,-I%,$(EFI_INC_PATHS))
EFI_CFLAGS	+= -DHAVE_USE_MS_ABI=1
EFI_CFLAGS	+= -ggdb

EFI_LDFLAGS	:= -nostdlib -znocombreloc -T $(EFI_LINKSCRIPT)
EFI_LDFLAGS	+= -shared -Bsymbolic
EFI_LDFLAGS	+= $(patsubst %,-L%,$(EFI_LIB_PATHS))

EFI_LIBS	:= -lefi -lgnuefi

what_to_build::	all

# build rules and macros
include build/build.mk

# declare applications here
$(call efi_app, hello, hello.c)
$(call efi_app, showmem, showmem.c)
$(call efi_app, fileio, fileio.c goodies.c)
$(call efi_app, osboot, osboot.c goodies.c libc.c)
$(call efi_app, snptest, snptest.c goodies.c libc.c inet6.c)

# generate a small IDE disk image for qemu
out/disk.img: $(APPS)
	@mkdir -p out
	$(QUIET)./build/mkdiskimg.sh $@
	@echo copying: $(APPS) README.txt to disk.img
	$(QUIET)mcopy -o -i out/disk.img@@1024K $(APPS) README.txt ::

ALL += out/disk.img

-include $(DEPS)

# ensure gnu-efi gets built
$(EFI_CRT0):
	@echo building: gnu-efi
	$(QUIET)$(MAKE) -C gnu-efi

QEMU_OPTS := -cpu qemu64
QEMU_OPTS += -bios ovmf/OVMF.fd
QEMU_OPTS += -drive file=out/disk.img,format=raw,if=ide

qemu:: all
	qemu-system-x86_64 $(QEMU_OPTS)

all: $(ALL)

clean::
	rm -rf out
