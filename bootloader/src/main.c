#include <stdio.h>
#include "banjuefi.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("BanJiuOS Boot Loader (Simulated EFI)\n");
    printf("=====================================\n\n");

    return banju_efi_main(NULL, NULL);
}
