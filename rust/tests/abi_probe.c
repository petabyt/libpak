#include "runtime.h"
#include "manifest.h"
#include "pak_gc.h"
#include <stddef.h>
#include <stdio.h>

int main(void) {
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %d %d\n",
        sizeof(struct PakModule), _Alignof(struct PakModule),
        offsetof(struct PakModule, rt), offsetof(struct PakModule, on_try_connect_usb),
        offsetof(struct PakModule, on_request_liveview_frame),
        offsetof(struct PakModule, on_custom_command),
        sizeof(struct PakWidget), _Alignof(struct PakWidget), offsetof(struct PakWidget, u),
        sizeof(struct PakFileHandle), sizeof(struct PakSavedConnection),
        sizeof(struct PakWiFiApFilter), sizeof(struct PakBtDevice),
        PAK_SCREEN_LIVEVIEW, PAK_USB);
    return 0;
}
