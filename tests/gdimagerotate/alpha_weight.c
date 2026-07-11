#include "gd.h"
#include "gdtest.h"

int main(void)
{
	gdImagePtr src, dst;
	int transparent;
	int x, y;
	int partial = 0;
	int dark = 0;

	src = gdImageCreateTrueColor(16, 16);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return gdNumFailures();
	}

	transparent = gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent);
	gdImageAlphaBlending(src, 0);
	gdImageFilledRectangle(src, 0, 0, 15, 15, transparent);
	gdImageFilledRectangle(src, 4, 4, 11, 11,
						   gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	gdImageSetInterpolationMethod(src, GD_LINEAR);

	dst = gdImageRotateInterpolated(src, 30.0f, transparent);
	gdTestAssert(dst != NULL);
	if (dst != NULL) {
		for (y = 0; y < gdImageSY(dst); y++) {
			for (x = 0; x < gdImageSX(dst); x++) {
				int pixel = gdImageGetTrueColorPixel(dst, x, y);
				int alpha = gdTrueColorGetAlpha(pixel);
				if (alpha > gdAlphaOpaque && alpha < gdAlphaTransparent) {
					partial++;
					if (gdTrueColorGetRed(pixel) < 220) {
						dark++;
					}
				}
			}
		}
		gdTestAssertMsg(partial > 0,
						"rotate should create partial-alpha pixels");
		gdTestAssertMsg(dark == 0,
						"rotate created %d dark partial-alpha pixels", dark);
		gdImageDestroy(dst);
	}

	gdImageDestroy(src);
	return gdNumFailures();
}
