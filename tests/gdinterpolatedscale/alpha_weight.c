#include "gd.h"
#include "gdtest.h"

static void assert_pixel_is_red_fade(int pixel, const char *label)
{
	int red = gdTrueColorGetRed(pixel);
	int green = gdTrueColorGetGreen(pixel);
	int blue = gdTrueColorGetBlue(pixel);
	int alpha = gdTrueColorGetAlpha(pixel);

	gdTestAssertMsg(red > 240, "%s: expected red > 240, got %d in %x", label,
					red, pixel);
	gdTestAssertMsg(green < 10, "%s: expected green < 10, got %d in %x",
					label, green, pixel);
	gdTestAssertMsg(blue < 10, "%s: expected blue < 10, got %d in %x", label,
					blue, pixel);
	gdTestAssertMsg(alpha > gdAlphaOpaque && alpha < gdAlphaTransparent,
					"%s: expected partial alpha, got %d in %x", label, alpha,
					pixel);
}

static void test_bilinear_truecolor(void)
{
	gdImagePtr src, dst;

	src = gdImageCreateTrueColor(2, 1);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}

	src->tpixels[0][0] = gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque);
	src->tpixels[0][1] = gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent);
	gdImageSetInterpolationMethod(src, GD_BILINEAR_FIXED);

	dst = gdImageScale(src, 3, 1);
	gdTestAssert(dst != NULL);
	if (dst != NULL) {
		assert_pixel_is_red_fade(gdImageGetTrueColorPixel(dst, 1, 0),
								 "bilinear truecolor");
		gdImageDestroy(dst);
	}
	gdImageDestroy(src);
}

static void test_bilinear_palette(void)
{
	gdImagePtr src, dst;
	int red, transparent;

	src = gdImageCreate(2, 1);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}

	red = gdImageColorAllocateAlpha(src, 255, 0, 0, gdAlphaOpaque);
	transparent = gdImageColorAllocateAlpha(src, 0, 0, 0, gdAlphaTransparent);
	gdImageSetPixel(src, 0, 0, red);
	gdImageSetPixel(src, 1, 0, transparent);
	gdImageSetInterpolationMethod(src, GD_BILINEAR_FIXED);

	dst = gdImageScale(src, 3, 1);
	gdTestAssert(dst != NULL);
	if (dst != NULL) {
		assert_pixel_is_red_fade(gdImageGetTrueColorPixel(dst, 1, 0),
								 "bilinear palette");
		gdImageDestroy(dst);
	}
	gdImageDestroy(src);
}

static void test_bicubic_fixed(void)
{
	gdImagePtr src, dst;
	int x, y;
	int partial = 0;
	int dark = 0;

	src = gdImageCreateTrueColor(4, 4);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}

	gdImageAlphaBlending(src, 0);
	gdImageFilledRectangle(src, 0, 0, 3, 3,
						   gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent));
	gdImageFilledRectangle(src, 0, 0, 1, 3,
						   gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	gdImageSetInterpolationMethod(src, GD_BICUBIC_FIXED);

	dst = gdImageScale(src, 8, 4);
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
						"bicubic fixed should create partial-alpha pixels");
		gdTestAssertMsg(dark == 0,
						"bicubic fixed created %d dark partial-alpha pixels",
						dark);
		gdImageDestroy(dst);
	}
	gdImageDestroy(src);
}

int main(void)
{
	test_bilinear_truecolor();
	test_bilinear_palette();
	test_bicubic_fixed();

	return gdNumFailures();
}
