#include "gd.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    gdUhdrError err;
    gdUhdrImagePtr im = gdUhdrImageCreateFromPtr((int)size, (void *)data,
                                                  GD_UHDR_FORMAT_JPEG, &err);
    if (im) {
        gdUhdrImageDestroy(im);
    }
    return 0;
}
