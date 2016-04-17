#include <efi.h>
#include <efilib.h>

static const CHAR16 *MemTypeName(UINT32 type, CHAR16 *buf) {
	switch (type) {
	case EfiReservedMemoryType:	return L"Reserved";
	case EfiLoaderCode:		return L"LoaderCode";
	case EfiLoaderData:		return L"LoaderData";
	case EfiBootServicesCode:	return L"BootSvcsCode";
	case EfiBootServicesData:	return L"BootSvcsData";
	case EfiRuntimeServicesCode:	return L"RunTimeCode";
	case EfiRuntimeServicesData:	return L"RunTimeData";
	case EfiConventionalMemory:	return L"Conventional";
	case EfiUnusableMemory:		return L"Unusable";
	case EfiACPIReclaimMemory:	return L"ACPIReclaim";
	case EfiACPIMemoryNVS:		return L"ACPINonVolMem";
	case EfiMemoryMappedIO:		return L"MemMappedIO";
	case EfiMemoryMappedIOPortSpace: return L"MemMappedPort";
	case EfiPalCode:		return L"PalCode";
	default:
		SPrint(buf, 32, L"0x%08x", type);
		return buf;
	}
}

static unsigned char scratch[4096];

static void dump_memmap(EFI_SYSTEM_TABLE *systab) {
	EFI_STATUS r;
	UINTN msize, off;
	EFI_MEMORY_DESCRIPTOR *mmap;
	UINTN mkey, dsize;
	UINT32 dversion;
	CHAR16 tmp[32];

	msize = sizeof(scratch);
	mmap = (EFI_MEMORY_DESCRIPTOR*) scratch;
	mkey = dsize = dversion;
	r = systab->BootServices->GetMemoryMap(&msize, mmap, &mkey, &dsize, &dversion);
	Print(L"r=%lx msz=%lx key=%lx dsz=%lx dvn=%lx\n",
		r, msize, mkey, dsize, dversion);	
	if (r != EFI_SUCCESS) {
		return;
	}
	for (off = 0; off < msize; off += dsize) {
		mmap = (EFI_MEMORY_DESCRIPTOR*) (scratch + off);
		Print(L"%016lx %016lx %08lx %c %04lx %s\n",
			mmap->PhysicalStart, mmap->VirtualStart,
			mmap->NumberOfPages, 
			mmap->Attribute & EFI_MEMORY_RUNTIME ? 'R' : '-',
			mmap->Attribute & 0xFFFF,
			MemTypeName(mmap->Type, tmp));
	}
}

EFI_STATUS efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
        InitializeLib(img, sys);
	dump_memmap(sys);
        return EFI_SUCCESS;
}
