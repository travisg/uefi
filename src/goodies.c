
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

void InitGoodies(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
	gSysTab = sys;
	gImage = img;
}

void WaitAnyKey(void) {
	SIMPLE_INPUT_INTERFACE *sii = gSysTab->ConIn;
	EFI_INPUT_KEY key;
	while (sii->ReadKeyStroke(sii, &key) != EFI_SUCCESS) ;
}

void Fatal(const CHAR16 *msg, EFI_STATUS status) {
	Print(L"\nERROR: %s (%lx)\n", msg, status);
	WaitAnyKey();
	gSysTab->BootServices->Exit(gImage, 1, StrSize(msg), (CHAR16*) msg);
}

CHAR16 *HandleToString(EFI_HANDLE h) {
	EFI_DEVICE_PATH *path = DevicePathFromHandle(h);
	if (path == NULL) return L"<NoPath>";
	CHAR16 *str = DevicePathToStr(path);
	if (str == NULL) return L"<NoString>";
	return str;
}
