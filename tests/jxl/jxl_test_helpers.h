#ifndef JXL_TEST_HELPERS_H
#define JXL_TEST_HELPERS_H

#include "gd.h"
#include "gdtest.h"

static inline gdImagePtr jxlTestCreatePattern(int alpha) {
	gdImagePtr im;
	int red, green, blue, semi;

	im = gdImageCreateTrueColor(32, 24);
	if (im == NULL) {
		return NULL;
	}

	gdImageAlphaBlending(im, 0);
	gdImageSaveAlpha(im, alpha);

	red = gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque);
	green = gdTrueColorAlpha(0, 255, 0, gdAlphaOpaque);
	blue = gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque);
	semi = gdTrueColorAlpha(255, 255, 0, 63);

	gdImageFilledRectangle(im, 0, 0, 31, 23, red);
	gdImageFilledRectangle(im, 4, 4, 27, 19, green);
	gdImageEllipse(im, 16, 12, 18, 10, blue);
	if (alpha) {
		gdImageFilledRectangle(im, 10, 6, 21, 17, semi);
	}

	return im;
}

static inline gdImagePtr jxlTestCreateSolid(int r, int g, int b, int a) {
	gdImagePtr im;
	int color;
	int x, y;

	im = gdImageCreateTrueColor(4, 4);
	if (im == NULL) {
		return NULL;
	}

	gdImageAlphaBlending(im, 0);
	gdImageSaveAlpha(im, 1);
	color = gdTrueColorAlpha(r, g, b, a);

	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			gdImageSetPixel(im, x, y, color);
		}
	}

	return im;
}

static inline void *jxlTestEncodeAnimationWithLoop(gdImagePtr first, int first_delay,
												   gdImagePtr second, int second_delay,
												   int lossless, int loop_count, int *size) {
	gdJxlWriteOptions options;
	gdJxlWritePtr writer;
	void *data = NULL;

	*size = 0;
	gdJxlWriteOptionsInit(&options);
	options.lossless = lossless;
	options.distance = lossless ? 0.0f : 1.0f;
	options.loopCount = loop_count;
	writer = gdJxlWriteOpenPtr(&options);
	if (writer == NULL) {
		return NULL;
	}

	if (!gdJxlWriteAddImage(writer, first, first_delay) ||
		!gdJxlWriteAddImage(writer, second, second_delay)) {
		gdJxlWritePtrFinish(writer, size);
		return NULL;
	}

	data = gdJxlWritePtrFinish(writer, size);
	return data;
}

static inline void *jxlTestEncodeAnimation(gdImagePtr first, int first_delay,
										   gdImagePtr second, int second_delay,
										   int lossless, int *size) {
	return jxlTestEncodeAnimationWithLoop(first, first_delay, second, second_delay,
										  lossless, 0, size);
}

#endif
