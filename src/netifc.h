#pragma once

int netifc_open(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys);
void netifc_close(void);
void netifc_poll(void);

