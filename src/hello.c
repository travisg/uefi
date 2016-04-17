
#include <efi.h>
#include <efilib.h>

EFI_STATUS efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
        SIMPLE_TEXT_OUTPUT_INTERFACE *ConOut = sys->ConOut;

        InitializeLib(img, sys);

	ConOut->OutputString(ConOut, L"Hello, EFI World!\n");

        return EFI_SUCCESS;
}
