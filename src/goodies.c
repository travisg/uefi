
#include <efi.h>
#include <efilib.h>

#include "goodies.h"

// Useful GUID Constants Not Defined by -lefi
EFI_GUID SimpleFileSystemProtocol = SIMPLE_FILE_SYSTEM_PROTOCOL;
EFI_GUID FileInfoGUID = EFI_FILE_INFO_ID;

// -lefi has its own globals, but this may end up not
// depending on that, so let's not depend on those
static EFI_SYSTEM_TABLE *gSysTab;
static EFI_HANDLE gImage;
static EFI_BOOT_SERVICES *gBootSvc;

void InitGoodies(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
	gSysTab = sys;
	gImage = img;
	gBootSvc = sys->BootServices;
}

void WaitAnyKey(void) {
	SIMPLE_INPUT_INTERFACE *sii = gSysTab->ConIn;
	EFI_INPUT_KEY key;
	while (sii->ReadKeyStroke(sii, &key) != EFI_SUCCESS) ;
}

void Fatal(const CHAR16 *msg, EFI_STATUS status) {
	Print(L"\nERROR: %s (%lx)\n", msg, status);
	WaitAnyKey();

	// Attempt to pass the message as our exit reason string
	UINTN exitdatasize = StrSize(msg);
	CHAR16 *exitdata;
	if (gSysTab->BootServices->AllocatePool(
		EfiLoaderData, exitdatasize, (void**) &exitdata)) {
		exitdatasize = 0;
		exitdata = NULL;
	} else {
		StrCpy(exitdata, msg);
	}
	gSysTab->BootServices->Exit(gImage, 1, exitdatasize, exitdata);
}

CHAR16 *HandleToString(EFI_HANDLE h) {
	EFI_DEVICE_PATH *path = DevicePathFromHandle(h);
	if (path == NULL) return L"<NoPath>";
	CHAR16 *str = DevicePathToStr(path);
	if (str == NULL) return L"<NoString>";
	return str;
}


EFI_STATUS OpenProtocol(EFI_HANDLE h, EFI_GUID *guid, void **ifc) {
	return gBootSvc->OpenProtocol(h, guid, ifc, gImage, NULL,
		EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
}

EFI_STATUS CloseProtocol(EFI_HANDLE h, EFI_GUID *guid) {
	return gBootSvc->CloseProtocol(h, guid, gImage, NULL);
}

void *LoadFile(CHAR16 *filename, UINTN *_sz) {
	EFI_LOADED_IMAGE *loaded;
	EFI_STATUS r;
	void *data = NULL;

	r = OpenProtocol(gImage, &LoadedImageProtocol, (void**) &loaded);
	if (r) {
		Print(L"Cannot open LoadedImageProtocol (%x)\n", r);
		goto exit0;
	}

	Print(L"Img DeviceHandle='%s'\n", HandleToString(loaded->DeviceHandle));
	Print(L"Img FilePath='%s'\n", DevicePathToStr(loaded->FilePath));
	Print(L"Img Base=%lx Size=%lx\n", loaded->ImageBase, loaded->ImageSize);

	EFI_FILE_IO_INTERFACE *fioi;
	r = OpenProtocol(loaded->DeviceHandle, &SimpleFileSystemProtocol, (void **) &fioi);
	if (r) {
		Print(L"Cannot open SimpleFileSystemProtocol (%x)\n", r);
		goto exit1;
	}

	EFI_FILE_HANDLE root;
	r = fioi->OpenVolume(fioi, &root);
	if (r) {
		Print(L"Cannot open root volume (%x)\n", r);
		goto exit2;
	}

	EFI_FILE_HANDLE file;
	r = root->Open(root, &file, filename, EFI_FILE_MODE_READ, 0);
	if (r) {
		Print(L"Cannot open file '%s' (%x)\n", filename, r);
		goto exit3;
	}

	char buf[512];
	UINTN sz = sizeof(buf);
	EFI_FILE_INFO *finfo = (void*) buf;
	r = file->GetInfo(file, &FileInfoGUID, &sz, finfo);
	if (r) {
		Print(L"Cannot get FileInfo (%x)\n", r);
		goto exit3;
	}

	r = gBootSvc->AllocatePool(EfiLoaderData, finfo->FileSize, (void**) &data);
	if (r) {
		Print(L"Cannot allocate buffer (%x)\n", r);
		data = NULL;
		goto exit4;
	}

	sz = finfo->FileSize;
	r = file->Read(file, &sz, data);
	if (r) {
		Print(L"Error reading file (%x)\n", r);
		gBootSvc->FreePool(data);
		data = NULL;
		goto exit4;
	}
	if (sz != finfo->FileSize) {
		Print(L"Short read\n");
		gBootSvc->FreePool(data);
		data = NULL;
		goto exit4;
	}
	*_sz = finfo->FileSize;
exit4:
	file->Close(file);
exit3:
	root->Close(root);
exit2:
	CloseProtocol(loaded->DeviceHandle, &SimpleFileSystemProtocol);
exit1:
	CloseProtocol(gImage, &LoadedImageProtocol);
exit0:
	return data;
}

