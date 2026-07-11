#include "gd.h"
#include "gdtest.h"

static void fill_black_white_columns(gdImagePtr im, int bright_width)
{
	int x, y;

	for (y = 0; y < gdImageSY(im); y++) {
		for (x = 0; x < gdImageSX(im); x++) {
			const int v = x < bright_width ? 255 : 0;
			im->tpixels[y][x] = gdTrueColorAlpha(v, v, v, gdAlphaOpaque);
		}
	}
}

static void assert_gray_between(int pixel, int min, int max, const char *label)
{
	int red = gdTrueColorGetRed(pixel);
	int green = gdTrueColorGetGreen(pixel);
	int blue = gdTrueColorGetBlue(pixel);

	gdTestAssertMsg(red >= min && red <= max,
					"%s: expected red between %d and %d, got %d in %x", label,
					min, max, red, pixel);
	gdTestAssertMsg(green >= min && green <= max,
					"%s: expected green between %d and %d, got %d in %x",
					label, min, max, green, pixel);
	gdTestAssertMsg(blue >= min && blue <= max,
					"%s: expected blue between %d and %d, got %d in %x", label,
					min, max, blue, pixel);
}

static void test_fixed_family_downscale_uses_triangle(void)
{
	const gdInterpolationMethod methods[] = {
		GD_BILINEAR_FIXED,
		GD_LINEAR,
		GD_BICUBIC_FIXED,
		GD_BICUBIC,
	};
	size_t i;

	for (i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
		gdImagePtr src, dst;

		src = gdImageCreateTrueColor(4, 1);
		gdTestAssert(src != NULL);
		if (src == NULL) {
			return;
		}
		fill_black_white_columns(src, 2);
		gdImageSetInterpolationMethod(src, methods[i]);

		dst = gdImageScale(src, 1, 1);
		gdTestAssert(dst != NULL);
		if (dst != NULL) {
			assert_gray_between(gdImageGetTrueColorPixel(dst, 0, 0), 216, 220,
								"fixed-family downscale");
			gdTestAssertMsg(gdImageGetInterpolationMethod(src) == methods[i],
							"source interpolation method was mutated");
			gdTestAssertMsg(gdImageGetInterpolationMethod(dst) == methods[i],
							"result interpolation method changed");
			gdImageDestroy(dst);
		}
		gdImageDestroy(src);
	}
}

static void test_default_created_downscale_uses_triangle(void)
{
	gdImagePtr src, dst;

	src = gdImageCreateTrueColor(4, 1);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}
	fill_black_white_columns(src, 2);

	dst = gdImageScale(src, 1, 1);
	gdTestAssert(dst != NULL);
	if (dst != NULL) {
		assert_gray_between(gdImageGetTrueColorPixel(dst, 0, 0), 216, 220,
							"default-created downscale");
		gdTestAssertMsg(gdImageGetInterpolationMethod(src) == GD_BILINEAR_FIXED,
						"default source interpolation method was mutated");
		gdTestAssertMsg(gdImageGetInterpolationMethod(dst) == GD_BILINEAR_FIXED,
						"default result interpolation method changed");
		gdImageDestroy(dst);
	}
	gdImageDestroy(src);
}

static void test_mixed_scale_uses_triangle(void)
{
	gdImagePtr src, dst;

	src = gdImageCreateTrueColor(4, 1);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}
	fill_black_white_columns(src, 2);
	gdImageSetInterpolationMethod(src, GD_LINEAR);

	dst = gdImageScale(src, 1, 3);
	gdTestAssert(dst != NULL);
	if (dst != NULL) {
		assert_gray_between(gdImageGetTrueColorPixel(dst, 0, 1), 216, 220,
							"mixed scale");
		gdImageDestroy(dst);
	}
	gdImageDestroy(src);
}

static void test_fixed_family_upscale_keeps_fast_paths(void)
{
	gdImagePtr src, bilinear, bicubic;

	src = gdImageCreateTrueColor(2, 1);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}
	fill_black_white_columns(src, 1);

	gdImageSetInterpolationMethod(src, GD_LINEAR);
	bilinear = gdImageScale(src, 3, 1);
	gdTestAssert(bilinear != NULL);
	if (bilinear != NULL) {
		assert_gray_between(gdImageGetTrueColorPixel(bilinear, 1, 0), 84, 86,
							"linear upscale");
		gdImageDestroy(bilinear);
	}

	gdImageSetInterpolationMethod(src, GD_BICUBIC);
	bicubic = gdImageScale(src, 3, 1);
	gdTestAssert(bicubic != NULL);
	if (bicubic != NULL) {
		const int red = gdTrueColorGetRed(gdImageGetTrueColorPixel(bicubic, 1, 0));
		gdTestAssertMsg(red < 180,
						"bicubic upscale should stay on fixed path, got red %d",
						red);
		gdImageDestroy(bicubic);
	}

	gdImageDestroy(src);
}

static void test_explicit_generic_filter_is_respected(void)
{
	gdImagePtr src, box, triangle;

	src = gdImageCreateTrueColor(4, 1);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}
	fill_black_white_columns(src, 1);

	gdImageSetInterpolationMethod(src, GD_BOX);
	box = gdImageScale(src, 1, 1);
	gdTestAssert(box != NULL);
	if (box != NULL) {
		assert_gray_between(gdImageGetTrueColorPixel(box, 0, 0), 154, 158,
							"explicit box downscale");
		gdImageDestroy(box);
	}

	gdImageSetInterpolationMethod(src, GD_TRIANGLE);
	triangle = gdImageScale(src, 1, 1);
	gdTestAssert(triangle != NULL);
	if (triangle != NULL) {
		assert_gray_between(gdImageGetTrueColorPixel(triangle, 0, 0), 168, 172,
							"explicit triangle downscale");
		gdImageDestroy(triangle);
	}

	gdImageDestroy(src);
}

static void test_nearest_is_preserved(void)
{
	gdImagePtr src, down, up;

	src = gdImageCreateTrueColor(4, 1);
	gdTestAssert(src != NULL);
	if (src == NULL) {
		return;
	}
	fill_black_white_columns(src, 2);
	gdImageSetInterpolationMethod(src, GD_NEAREST_NEIGHBOUR);

	down = gdImageScale(src, 1, 1);
	gdTestAssert(down != NULL);
	if (down != NULL) {
		gdTestAssertMsg(gdTrueColorGetRed(gdImageGetTrueColorPixel(down, 0, 0)) ==
							255,
						"nearest downscale should preserve nearest sample");
		gdImageDestroy(down);
	}

	up = gdImageScale(src, 8, 1);
	gdTestAssert(up != NULL);
	if (up != NULL) {
		gdTestAssertMsg(gdTrueColorGetRed(gdImageGetTrueColorPixel(up, 2, 0)) ==
							255,
						"nearest upscale should preserve nearest sample");
		gdImageDestroy(up);
	}

	gdImageDestroy(src);
}

int main(void)
{
	test_fixed_family_downscale_uses_triangle();
	test_default_created_downscale_uses_triangle();
	test_mixed_scale_uses_triangle();
	test_fixed_family_upscale_keeps_fast_paths();
	test_explicit_generic_filter_is_respected();
	test_nearest_is_preserved();

	return gdNumFailures();
}
