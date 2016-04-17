
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

