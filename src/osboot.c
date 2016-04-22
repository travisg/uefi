#include <efi.h>
#include <efilib.h>

#include "goodies.h"

#define E820_IGNORE	0
#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3
#define E820_NVS	4
#define E820_UNUSABLE	5

const CHAR16 *e820name[] = {
	L"IGNORE",
	L"RAM",
	L"RESERVED",
	L"ACPI",
	L"NVS",
	L"UNUSABLE",
};

struct e820entry {
	UINT64 addr;
	UINT64 size;
	UINT32 type;
} __attribute__((packed));

unsigned e820type(unsigned uefi_mem_type) {
	switch (uefi_mem_type) {
	case EfiReservedMemoryType:
	case EfiPalCode:
		return E820_RESERVED;
	case EfiRuntimeServicesCode:
	case EfiRuntimeServicesData:
#if WITH_RUNTIME_SERVICES
		return E820_RESERVED;
#else
		return E820_RAM;
#endif
	case EfiACPIReclaimMemory:
		return E820_ACPI;
	case EfiACPIMemoryNVS:
		return E820_NVS;
	case EfiLoaderCode:
	case EfiLoaderData:
	case EfiBootServicesCode:
	case EfiBootServicesData:
	case EfiConventionalMemory:
		return E820_RAM;
	case EfiMemoryMappedIO:
	case EfiMemoryMappedIOPortSpace:
		return E820_IGNORE;
	default:
		if (uefi_mem_type >= 0x80000000) {
			return E820_RAM;
		}
		return E820_UNUSABLE;
	}
}

static unsigned char scratch[8192];
static struct e820entry e820table[128];

int process_memory_map(EFI_SYSTEM_TABLE *sys, UINTN *_key, int silent) {
	EFI_MEMORY_DESCRIPTOR *mmap;
	struct e820entry *entry = e820table;
	UINTN msize, off;
	UINTN mkey, dsize;
	UINT32 dversion;
	unsigned n, type;
	EFI_STATUS r;

	msize = sizeof(scratch);
	mmap = (EFI_MEMORY_DESCRIPTOR*) scratch;
	mkey = dsize = dversion = 0;
	r = sys->BootServices->GetMemoryMap(&msize, mmap, &mkey, &dsize, &dversion);
	if (!silent) Print(L"r=%lx msz=%lx key=%lx dsz=%lx dvn=%lx\n", r, msize, mkey, dsize, dversion);	
	if (r != EFI_SUCCESS) {
		return -1;
	}
	if ((msize / dsize) > 128) {
		if (!silent) Print(L"Memory Table Too Large (%ld entries)\n", (msize / dsize));
		return -1;
	}
	for (off = 0, n = 0; off < msize; off += dsize) {
		mmap = (EFI_MEMORY_DESCRIPTOR*) (scratch + off);
		type = e820type(mmap->Type);
		if (type == E820_IGNORE) {
			continue;
		}
		if ((n > 0) && (entry[n-1].type == type)) {
			if ((entry[n-1].addr + entry[n-1].size) == mmap->PhysicalStart) {
				entry[n-1].size += mmap->NumberOfPages * 4096UL;
				continue;
			}
		}
		entry[n].addr = mmap->PhysicalStart;
		entry[n].size = mmap->NumberOfPages * 4096UL;
		entry[n].type = type;
		n++;
	}
	*_key = mkey;
	return n;
}

#define ZP_E820_COUNT		0x1E8	// byte
#define ZP_SETUP		0x1F1	// start of setup structure
#define ZP_SETUP_SECTS		0x1F1	// byte (setup_size/512-1)
#define ZP_JUMP			0x200   // jump instruction
#define ZP_HEADER		0x202	// word "HdrS"
#define ZP_VERSION		0x206	// half 0xHHLL
#define ZP_LOADER_TYPE		0x210	// byte
#define ZP_RAMDISK_BASE		0x218	// word (ptr or 0)
#define ZP_RAMDISK_SIZE		0x21C	// word (bytes)
#define ZP_EXTRA_MAGIC		0x220	// word 
#define ZP_CMDLINE		0x228	// word (ptr)
#define ZP_SYSSIZE		0x1F4	// word (size/16)
#define ZP_XLOADFLAGS		0x236	// half
#define ZP_E820_TABLE		0x2D0	// 128 entries

#define ZP_ACPI_RSD		0x080   // word phys ptr
#define ZP_FB_BASE		0x090
#define ZP_FB_WIDTH		0x094
#define ZP_FB_HEIGHT		0x098
#define ZP_FB_STRIDE		0x09C
#define ZP_FB_FORMAT		0x0A0

#define ZP_MAGIC_VALUE		0xDBC64323

#define ZP8(p,off)	(*((UINT8*)((p) + (off))))
#define ZP16(p,off)	(*((UINT16*)((p) + (off))))
#define ZP32(p,off)	(*((UINT32*)((p) + (off))))

typedef struct {
	UINT8 *zeropage;
	UINT8 *cmdline;
	void *image;
	UINT32 pages;
} kernel_t;

void install_memmap(kernel_t *k, struct e820entry *memmap, unsigned count) {
	unsigned char *src = (void *) memmap;
	unsigned char *dst = (void *) (k->zeropage + ZP_E820_TABLE);
	unsigned sz = sizeof(*memmap) * count;
	while (sz > 0) {
		*dst++ = *src++;
		sz--;
	}
	ZP8(k->zeropage, ZP_E820_COUNT) = count;
}

void start_kernel(kernel_t *k) {
	// 64bit entry is at offset 0x200
	UINT64 entry = (UINT64) (k->image + 0x200);

	// ebx = 0, ebp = 0, edi = 0, esi = zeropage
	__asm__ __volatile__ (
	"movl $0, %%ebp \n"
	"cli \n"
	"jmp *%[entry] \n"
	:: [entry]"a"(entry),
	   [zeropage] "S"(k->zeropage),
	   "b"(0), "D"(0)
	);
	for (;;) ;
}

int load_kernel(EFI_BOOT_SERVICES *bs, CHAR16 *fn, kernel_t *k) {
	UINTN sz;
	UINT8 *image;
	UINT32 setup_sz;
	UINT32 image_sz;
	UINT32 setup_end;
	EFI_PHYSICAL_ADDRESS mem;

	k->zeropage = NULL;
	k->cmdline = NULL;
	k->image = NULL;
	k->pages = 0;

	image = LoadFile(fn, &sz);
	if (image == NULL) {
		return -1;
	}

	if (sz < 1024) {
		// way too small to be a kernel
		goto fail;
	}

	if (ZP32(image, ZP_HEADER) != 0x53726448) {
		Print(L"kernel: invalid setup magic %08x\n", ZP32(image, ZP_HEADER));
		goto fail;
	}
	if (ZP16(image, ZP_VERSION) < 0x020B) {
		Print(L"kernel: unsupported setup version %04x\n", ZP16(image, ZP_VERSION));
		goto fail;
	}
	setup_sz = (ZP8(image, ZP_SETUP_SECTS) + 1) * 512;
	image_sz = (ZP16(image, ZP_SYSSIZE) * 16);
	setup_end = ZP_JUMP + ZP8(image, ZP_JUMP+1);

	Print(L"setup %d image %d  hdr %04x-%04x\n", setup_sz, image_sz, ZP_SETUP, setup_end);
	// image size may be rounded up, thus +15
	if ((setup_sz < 1024) || ((setup_sz + image_sz) > (sz + 15))) {
		Print(L"kernel: invalid image size\n");
		goto fail;
	}

	mem = 0x1000;
	if (bs->AllocatePages(AllocateAddress, EfiLoaderData, 1, &mem)) {
		Print(L"kernel: cannot allocate 'zero page'\n");
		goto fail;
	}
	k->zeropage = (void*) mem;

	mem = 0x2000;
	if (bs->AllocatePages(AllocateAddress, EfiLoaderData, 1, &mem)) {
		Print(L"kernel: cannot allocate commandline\n");
		goto fail;
	}
	k->cmdline = (void*) mem;

	mem = 0x100000;
	k->pages = (image_sz + 4095) / 4096;
	if (bs->AllocatePages(AllocateAddress, EfiLoaderData, k->pages + 1, &mem)) {
		Print(L"kernel: cannot allocate kernel\n");
		goto fail;
	}
	k->image = (void*) mem;

	// setup zero page, copy setup header from kernel binary
	ZeroMem(k->zeropage, 4096);
	CopyMem(k->zeropage + ZP_SETUP, image + ZP_SETUP, setup_end - ZP_SETUP);

	CopyMem(k->image, image + setup_sz, image_sz);

	// empty commandline for now
	ZP32(k->zeropage, ZP_CMDLINE) = 0x2000;
	k->cmdline[0] = 0;

	// no ramdisk for now
	ZP32(k->zeropage, ZP_RAMDISK_BASE) = 0;
	ZP32(k->zeropage, ZP_RAMDISK_SIZE) = 0;

	// undefined bootloader
	ZP8(k->zeropage, ZP_LOADER_TYPE) = 0xFF;

	Print(L"kernel @%lx, zeropage @%lx, cmdline @%lx\n",
		k->image, k->zeropage, k->cmdline);

	bs->FreePool(image);
	return 0;
fail:
	bs->FreePool(image);
	if (k->image) {
		bs->FreePages((EFI_PHYSICAL_ADDRESS) k->image, k->pages);
	}
	if (k->cmdline) {
		bs->FreePages((EFI_PHYSICAL_ADDRESS) k->cmdline, 1);
	}
	if (k->zeropage) {
		bs->FreePages((EFI_PHYSICAL_ADDRESS) k->zeropage, 1);
	}

	return -1;
}

static EFI_GUID GraphicsOutputProtocol = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

void dump_graphics_modes(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	UINTN sz;
	UINT32 num;
	for (num = 0; num < gop->Mode->MaxMode; num++) {
		if (gop->QueryMode(gop, num, &sz, &info)) {
			continue;
		}
		Print(L"Mode %d  %d x %d (stride %d) fmt %d\n",
			num, info->HorizontalResolution, info->VerticalResolution,
			info->PixelsPerScanLine, info->PixelFormat);
		if (info->PixelFormat == PixelBitMask) {
			Print(L"Mode %d R:%08x G:%08x B:%08x X:%08x\n", num,
				info->PixelInformation.RedMask,
				info->PixelInformation.GreenMask,
				info->PixelInformation.BlueMask,
				info->PixelInformation.ReservedMask);
		}
	}
}

static EFI_GUID AcpiTableGUID = ACPI_TABLE_GUID;
static EFI_GUID Acpi2TableGUID = ACPI_20_TABLE_GUID;

static UINT8 ACPI_RSD_PTR[8] = "RSD PTR ";

void *find_acpi_root(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
	EFI_CONFIGURATION_TABLE *cfgtab = sys->ConfigurationTable;
	int i;

	for (i = 0; i < sys->NumberOfTableEntries; i++) {
		if (!CompareGuid(&cfgtab[i].VendorGuid, &AcpiTableGUID) &&
			!CompareGuid(&cfgtab[i].VendorGuid, &Acpi2TableGUID)) {
			// not an ACPI table
			continue;
		}
		if (CompareMem(cfgtab[i].VendorTable, ACPI_RSD_PTR, 8)) {
			// not the Root Description Pointer
			continue;
		}
		return cfgtab[i].VendorTable;
	}
	return NULL;
}

EFI_STATUS efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
        InitializeLib(img, sys);
	InitGoodies(img, sys);
	EFI_STATUS r;
	kernel_t kernel;
	UINTN key;
	int n;

	Print(L"OSBOOT v0.1\n\n");

	find_acpi_root(img, sys);

	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	r = sys->BootServices->LocateProtocol(&GraphicsOutputProtocol, NULL, (void**) &gop);
	Print(L"Framebuffer base is at %lx\n\n", gop->Mode->FrameBufferBase);

	if (load_kernel(sys->BootServices, L"lk.bin", &kernel)) {
		Print(L"Failed to load kernel image\n");
		goto fail;
	}

	ZP32(kernel.zeropage, ZP_EXTRA_MAGIC) = ZP_MAGIC_VALUE;
	ZP32(kernel.zeropage, ZP_ACPI_RSD) = (UINT32) find_acpi_root(img, sys);

	ZP32(kernel.zeropage, ZP_FB_BASE) = (UINT32) gop->Mode->FrameBufferBase;
	ZP32(kernel.zeropage, ZP_FB_WIDTH) = (UINT32) gop->Mode->Info->HorizontalResolution;
	ZP32(kernel.zeropage, ZP_FB_HEIGHT) = (UINT32) gop->Mode->Info->VerticalResolution;
	ZP32(kernel.zeropage, ZP_FB_STRIDE) = (UINT32) gop->Mode->Info->PixelsPerScanLine;
	ZP32(kernel.zeropage, ZP_FB_FORMAT) = (UINT32) gop->Mode->Info->PixelFormat;

	n = process_memory_map(sys, &key, 0);
	if (n > 0) {
		struct e820entry *e = e820table;
		while (n > 0) {
			Print(L"%016lx %016lx %s\n", e->addr, e->size, e820name[e->type]);
			e++;
			n--;
		}
	}

	r = sys->BootServices->ExitBootServices(img, key);
	if (r == EFI_INVALID_PARAMETER) {
		n = process_memory_map(sys, &key, 1);
		r = sys->BootServices->ExitBootServices(img, key);
		if (r) {
			Print(L"Cannot Exit Services! %x\n", r);
		} else {
			install_memmap(&kernel, e820table, n);
			start_kernel(&kernel);
		}
	} else if (r) {
		Print(L"Cannot Exit! %x\n", r);
	} else {
		for (;;) ;
	}
fail:
	WaitAnyKey();
	return EFI_SUCCESS;
}
