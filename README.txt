What is This?
-------------

This project contains some experiments in software that runs on UEFI
firmware for the purpose of exploring UEFI development and bootloader
development.  It might someday become a bootloader or something fancier
but right now it's just a sandbox and a place for the author to keep
all the bits necessary to build and debug such software handy, and
track pointers to useful documentation.


External Software Included
--------------------------

Local Path:   gnu-efi/...
Description:  headers and tooling to build UEFI binaries with gcc, etc
Project:      https://sourceforge.net/projects/gnu-efi/
Source:       git://git.code.sf.net/p/gnu-efi/code
Version:      6605c16fc8b1fd3b2085364902d1fa73aa7fad76 (post-3.0.4)
License:      BSD-ish, see gnu-efi/README.*

Local Path:   ovmf/... 
Description:  UEFI Firmware Suitable for use in Qemu
Distribution: http://www.tianocore.org/ovmf/
Version:      OVMF-X64-r15214.zip
License:      BSD-ish, see ovmf/LICENSE


External Dependencies
---------------------

qemu-system-x86_64 is needed to test in emulation
gnu parted and mtools are needed to generate the disk.img for Qemu


Useful Resources & Documentation
--------------------------------

ACPI & UEFI Specifications 
http://www.uefi.org/specifications

Intel 64 and IA-32 Architecture Manuals
http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html

Tianocore UEFI Open Source Community
(Source for OVMF, EDK II Dev Environment, etc)
http://www.tianocore.org/
https://github.com/tianocore


Thanks
------

Matthew Garrett's "Getting Started With UEFI Development" was very useful
as a guide to the use of the gcc toolchain for building with gnu-efi.
https://mjg59.dreamwidth.org/18773.html

The osdev.org wiki had some useful recipes for generating disk images
suitable for testing with Qemu and tricks and tips for debugging UEFI apps.
http://wiki.osdev.org/UEFI
http://wiki.osdev.org/Debugging_UEFI_applications_with_GDB

