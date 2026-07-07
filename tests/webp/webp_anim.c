#include "gd.h"
#include "gdtest.h"
#include <string.h>

static gdImagePtr make_frame(int color) {
	gdImagePtr im = gdImageCreateTrueColor(4, 4);
	int x, y;

	if (im == NULL) {
		return NULL;
	}
	gdImageAlphaBlending(im, 0);
	gdImageSaveAlpha(im, 1);
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			gdImageSetPixel(im, x, y, color);
		}
	}
	return im;
}

static void assert_pixel_rgb(gdImagePtr im, int x, int y, int r, int g, int b) {
	int c = gdImageGetPixel(im, x, y);

	gdTestAssertMsg(gdImageRed(im, c) == r && gdImageGreen(im, c) == g &&
						gdImageBlue(im, c) == b,
					"pixel (%d,%d) is %d,%d,%d, expected %d,%d,%d", x, y,
					gdImageRed(im, c), gdImageGreen(im, c), gdImageBlue(im, c),
					r, g, b);
}

int main() {
	gdWebpWriteOptions options;
	gdWebpWritePtr writer;
	gdWebpReadPtr reader;
	gdWebpInfo info;
	gdWebpFrameInfo frameInfo;
	gdImagePtr red, blue, frame0 = NULL, frame1 = NULL;
	gdImagePtr image0 = NULL, image1 = NULL;
	void *data;
	int size = 0;

	memset(&options, 0, sizeof(options));
	options.canvasWidth = 4;
	options.canvasHeight = 4;
	options.loopCount = 3;
	options.quality = gdWebpLossless;

	red = make_frame(gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	blue = make_frame(gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	if (red == NULL || blue == NULL) {
		return 1;
	}

	writer = gdWebpWriteOpenPtr(&options);
	gdTestAssert(writer != NULL);
	gdTestAssert(gdWebpWriteAddImage(writer, red, 120));
	gdTestAssert(gdWebpWriteAddImage(writer, blue, 80));
	data = gdWebpWritePtrFinish(writer, &size);
	gdTestAssert(data != NULL);
	gdTestAssert(size > 0);

	gdTestAssert(gdWebpIsAnimatedPtr(size, data) == 1);
	reader = gdWebpReadOpenPtr(size, data);
	gdTestAssert(reader != NULL);
	gdTestAssert(gdWebpReadGetInfo(reader, &info));
	gdTestAssert(info.width == 4);
	gdTestAssert(info.height == 4);
	gdTestAssert(info.frameCount == 2);
	gdTestAssert(info.loopCount == 3);

	gdTestAssert(gdWebpReadNextFrame(reader, &frameInfo, &frame0) == 1);
	gdTestAssert(frameInfo.frameIndex == 0);
	gdTestAssert(frameInfo.duration == 120);
	gdTestAssert(frame0 != NULL);
	if (frame0 != NULL) {
		gdTestAssert(gdImageSX(frame0) == 4);
		gdTestAssert(gdImageSY(frame0) == 4);
		assert_pixel_rgb(frame0, 0, 0, 255, 0, 0);
	}
	gdTestAssert(gdWebpReadNextFrame(reader, &frameInfo, &frame1) == 1);
	gdTestAssert(frameInfo.frameIndex == 1);
	gdTestAssert(frameInfo.duration == 80);
	gdTestAssert(frame1 != NULL);
	if (frame1 != NULL) {
		assert_pixel_rgb(frame1, 0, 0, 0, 0, 255);
	}
	if (frame0 != NULL) {
		assert_pixel_rgb(frame0, 0, 0, 255, 0, 0);
		gdImageDestroy(frame0);
	}
	if (frame1 != NULL) {
		gdImageDestroy(frame1);
	}
	gdTestAssert(gdWebpReadNextFrame(reader, &frameInfo, NULL) == 0);
	gdWebpReadClose(reader);

	reader = gdWebpReadOpenPtr(size, data);
	gdTestAssert(reader != NULL);
	gdTestAssert(gdWebpReadNextImage(reader, &frameInfo, &image0) == 1);
	gdTestAssert(frameInfo.frameIndex == 0);
	if (image0 != NULL) {
		assert_pixel_rgb(image0, 0, 0, 255, 0, 0);
	}
	gdTestAssert(gdWebpReadNextImage(reader, &frameInfo, &image1) == 1);
	gdTestAssert(frameInfo.frameIndex == 1);
	if (image1 != NULL) {
		assert_pixel_rgb(image1, 0, 0, 0, 0, 255);
	}
	if (image0 != NULL) {
		assert_pixel_rgb(image0, 0, 0, 255, 0, 0);
		gdImageDestroy(image0);
	}
	if (image1 != NULL) {
		gdImageDestroy(image1);
	}
	gdWebpReadClose(reader);

	gdTestAssert(gdWebpIsAnimatedPtr(4, "nope") == -1);

	gdFree(data);
	gdImageDestroy(red);
	gdImageDestroy(blue);
	return 0;
}
