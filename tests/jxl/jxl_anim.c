#include "gd.h"
#include "gdtest.h"
#include "jxl_test_helpers.h"

int main() {
	gdImagePtr red, blue, frame;
	gdJxlReadPtr reader;
	gdJxlInfo info;
	void *data;
	int size = 0;
	int delay = -1;

	red = jxlTestCreateSolid(255, 0, 0, gdAlphaOpaque);
	blue = jxlTestCreateSolid(0, 0, 255, gdAlphaOpaque);
	if (!gdTestAssert(red != NULL) || !gdTestAssert(blue != NULL))
		goto cleanup_images;

	data = jxlTestEncodeAnimationWithLoop(red, 120, blue, 80, 0, 3, &size);
	if (!gdTestAssert(data != NULL) || !gdTestAssert(size > 0))
		goto cleanup_images;

	reader = gdJxlReadOpenPtr(size, data, NULL);
	if (!gdTestAssert(reader != NULL))
		goto cleanup_data;

	gdTestAssert(gdJxlReadGetInfo(reader, &info));
	gdTestAssert(info.loop_count == 3);

	gdTestAssert(gdJxlReadNextImage(reader, &delay, &frame) == 1);
	gdTestAssert(frame != NULL);
	gdTestAssert(delay == 120);
	gdTestAssert(gdImageSX(frame) == 4);
	gdTestAssert(gdImageSY(frame) == 4);
	gdImageDestroy(frame);

	gdTestAssert(gdJxlReadNextImage(reader, &delay, &frame) == 1);
	gdTestAssert(frame != NULL);
	gdTestAssert(delay == 80);
	gdImageDestroy(frame);

	gdTestAssert(gdJxlReadNextImage(reader, &delay, &frame) == 0);
	gdTestAssert(frame == NULL);

	gdJxlReadClose(reader);
cleanup_data:
	gdFree(data);
cleanup_images:
	gdImageDestroy(red);
	gdImageDestroy(blue);

	return gdNumFailures();
}
