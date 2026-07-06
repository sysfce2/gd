#include "gd.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    gdImagePtr im = gdImageCreateFromGifPtr((int)size, (void *)data);
    if (im) {
        gdImageDestroy(im);
    }
    return 0;
}
