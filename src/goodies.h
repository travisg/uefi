#pragma once

void InitGoodies(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys);

void WaitAnyKey(void);
void Fatal(const CHAR16 *msg, EFI_STATUS status);
CHAR16 *HandleToString(EFI_HANDLE handle);

// GUIDs
extern EFI_GUID SimpleFileSystemProtocol;
extern EFI_GUID FileInfoGUID;
