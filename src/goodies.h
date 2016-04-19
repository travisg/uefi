#pragma once

void InitGoodies(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys);

void WaitAnyKey(void);
void Fatal(const CHAR16 *msg, EFI_STATUS status);
CHAR16 *HandleToString(EFI_HANDLE handle);

// Convenience wrappers for Open/Close protocol for use by
// UEFI app code that's not a driver model participant
EFI_STATUS OpenProtocol(EFI_HANDLE h, EFI_GUID *guid, void **ifc);
EFI_STATUS CloseProtocol(EFI_HANDLE h, EFI_GUID *guid);

void *LoadFile(CHAR16 *filename, UINTN *size_out);

// GUIDs
extern EFI_GUID SimpleFileSystemProtocol;
extern EFI_GUID FileInfoGUID;
