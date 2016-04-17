
#include <efi.h>
#include <efilib.h>

#include "goodies.h"

EFI_STATUS efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
        SIMPLE_TEXT_OUTPUT_INTERFACE *ConOut = sys->ConOut;
	EFI_BOOT_SERVICES *bs = sys->BootServices;
	EFI_LOADED_IMAGE *loaded;
	EFI_STATUS r;

        InitializeLib(img, sys);
	InitGoodies(img, sys);

	ConOut->OutputString(ConOut, L"Hello, EFI World!\r\n");

	r = bs->HandleProtocol(img, &LoadedImageProtocol, (void**) &loaded);
	if (r) Fatal(L"LoadedImageProtocol", r);

	Print(L"Img DeviceHandle='%s'\n", HandleToString(loaded->DeviceHandle));
	Print(L"Img FilePath='%s'\n", DevicePathToStr(loaded->FilePath));
	Print(L"Img Base=%lx Size=%lx\n", loaded->ImageBase, loaded->ImageSize);

	EFI_FILE_IO_INTERFACE *fioi;
	r = bs->HandleProtocol(loaded->DeviceHandle, &SimpleFileSystemProtocol, (void **) &fioi);
	if (r) Fatal(L"SimpleFileSystemProtocol", r);

	EFI_FILE_HANDLE root;
	r = fioi->OpenVolume(fioi, &root);
	if (r) Fatal(L"OpenVolume", r);

	EFI_FILE_HANDLE file;
	r = root->Open(root, &file, L"README.txt", EFI_FILE_MODE_READ, 0);
	if (r) Fatal(L"Open(README.txt)", r);

	char buf[4096];
	UINTN sz = sizeof(buf);
	EFI_FILE_INFO *finfo = (void*) buf;
	r = file->GetInfo(file, &FileInfoGUID, &sz, finfo);
	if (r) Fatal(L"GetInfo", r);
	Print(L"File %lx %lx %lx\n", finfo->Size, finfo->FileSize, finfo->PhysicalSize);

	sz = sizeof(buf) - 1;
	r = file->Read(file, &sz, buf);
	if (r) Fatal(L"Read", r);
	Print(L"BufSize %ld\n", sz);

	char *x = buf;
	while(sz-- > 0) Print(L"%c", (CHAR16) *x++);

	file->Close(file);
	root->Close(root);

	WaitAnyKey();
        return EFI_SUCCESS;
}
