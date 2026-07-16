#include "gd.h"
#include "gdtest.h"
#include "jxl_test_helpers.h"

int main() {
	gdImagePtr red, blue, frame;
	gdJxlReadPtr reader;
	void *data;
	int size = 0;
	int delay = -1;

	red = jxlTestCreateSolid(255, 0, 0, gdAlphaOpaque);
	blue = jxlTestCreateSolid(0, 0, 255, gdAlphaOpaque);
	if (!gdTestAssert(red != NULL) || !gdTestAssert(blue != NULL))
		goto cleanup_images;

	data = jxlTestEncodeAnimation(red, 120, blue, 80, 1, &size);
	if (!gdTestAssert(data != NULL) || !gdTestAssert(size > 0))
		goto cleanup_images;

	reader = gdJxlReadOpenPtr(size, data, NULL);
	if (!gdTestAssert(reader != NULL))
		goto cleanup_data;

	gdTestAssert(gdJxlReadNextImage(reader, &delay, &frame) == 1);
	gdTestAssert(frame != NULL);
	gdTestAssert(delay == 120);
	gdAssertImageEquals(red, frame);
	gdImageDestroy(frame);

	gdTestAssert(gdJxlReadNextImage(reader, &delay, &frame) == 1);
	gdTestAssert(frame != NULL);
	gdTestAssert(delay == 80);
	gdAssertImageEquals(blue, frame);
	gdImageDestroy(frame);

	gdJxlReadClose(reader);
cleanup_data:
	gdFree(data);
cleanup_images:
	gdImageDestroy(red);
	gdImageDestroy(blue);

	return gdNumFailures();
}
