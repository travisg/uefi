
#include <efi.h>
#include <efilib.h>

#include "goodies.h"

EFI_STATUS efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
	EFI_BOOT_SERVICES *bs = sys->BootServices;
	EFI_LOADED_IMAGE *loaded;
	EFI_STATUS r;

        InitializeLib(img, sys);
	InitGoodies(img, sys);

	Print(L"Hello, EFI World\n");

	r = OpenProtocol(img, &LoadedImageProtocol, (void**) &loaded);
	if (r) Fatal(L"LoadedImageProtocol", r);

	Print(L"Img DeviceHandle='%s'\n", HandleToString(loaded->DeviceHandle));
	Print(L"Img FilePath='%s'\n", DevicePathToStr(loaded->FilePath));
	Print(L"Img Base=%lx Size=%lx\n", loaded->ImageBase, loaded->ImageSize);

	EFI_FILE_IO_INTERFACE *fioi;
	r = OpenProtocol(loaded->DeviceHandle, &SimpleFileSystemProtocol, (void **) &fioi);
	if (r) Fatal(L"SimpleFileSystemProtocol", r);

	EFI_FILE_HANDLE root;
	r = fioi->OpenVolume(fioi, &root);
	if (r) Fatal(L"OpenVolume", r);

	EFI_FILE_HANDLE file;
	r = root->Open(root, &file, L"README.txt", EFI_FILE_MODE_READ, 0);

	if (r == EFI_SUCCESS) {
		char buf[512];
		UINTN sz = sizeof(buf);
		EFI_FILE_INFO *finfo = (void*) buf;
		r = file->GetInfo(file, &FileInfoGUID, &sz, finfo);
		if (r) Fatal(L"GetInfo", r);
		Print(L"FileSize %ld\n", finfo->FileSize);

		sz = sizeof(buf) - 1;
		r = file->Read(file, &sz, buf);
		if (r) Fatal(L"Read", r);

		char *x = buf;
		while(sz-- > 0) Print(L"%c", (CHAR16) *x++);

		file->Close(file);
	}

	root->Close(root);
	CloseProtocol(loaded->DeviceHandle, &SimpleFileSystemProtocol);
	CloseProtocol(img, &LoadedImageProtocol);

	WaitAnyKey();

        return EFI_SUCCESS;
}
