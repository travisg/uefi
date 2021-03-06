#!/bin/bash -e

if [ -z "$1" ]; then
	echo usage: $0 "<diskimg>"
	exit 1
fi

if [[ ! -f $1 ]]; then
	echo creating: $1
	dd if=/dev/zero of="$1" bs=512 count=93750

	parted "$1" -s -a minimal mklabel gpt
	parted "$1" -s -a minimal mkpart EFI FAT16 2048s 93716s
	parted "$1" -s -a minimal toggle 1 boot

	mformat -i "$1"@@1024K -h 32 -t 32 -n 64 -c 1
fi

