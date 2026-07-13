#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gd.h"
#include "gdtest.h"

static gdScaleOptions content_options(void)
{
	gdScaleOptions options;

	options.fit = GD_SCALE_FIT_COVER;
	options.gravity = GD_SCALE_GRAVITY_CENTER;
	options.strategy = GD_SCALE_STRATEGY_ENTROPY;
	options.background_color = gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent);
	options.interpolation = GD_NEAREST_NEIGHBOUR;

	return options;
}

static int pixel(gdImagePtr im, int x, int y)
{
	return gdImageGetTrueColorPixel(im, x, y);
}

static void assert_dimensions(gdImagePtr im, int width, int height)
{
	gdTestAssert(im != NULL);
	gdTestAssertMsg(gdImageSX(im) == width && gdImageSY(im) == height,
					"expected %dx%d, got %dx%d\n", width, height,
					im ? gdImageSX(im) : -1, im ? gdImageSY(im) : -1);
}

static int count_color(gdImagePtr im, int color)
{
	int x, y, count = 0;

	for (y = 0; y < gdImageSY(im); y++) {
		for (x = 0; x < gdImageSX(im); x++) {
			if (pixel(im, x, y) == color) {
				count++;
			}
		}
	}

	return count;
}

static int images_equal(gdImagePtr a, gdImagePtr b)
{
	int x, y;

	if (a == NULL || b == NULL || gdImageSX(a) != gdImageSX(b) ||
		gdImageSY(a) != gdImageSY(b)) {
		return 0;
	}

	for (y = 0; y < gdImageSY(a); y++) {
		for (x = 0; x < gdImageSX(a); x++) {
			if (pixel(a, x, y) != pixel(b, x, y)) {
				return 0;
			}
		}
	}

	return 1;
}

static gdImagePtr make_entropy_source(void)
{
	gdImagePtr im;
	int blue, black, white;
	int x, y;

	im = gdImageCreateTrueColor(16, 8);
	gdTestAssert(im != NULL);
	gdImageAlphaBlending(im, gdEffectReplace);
	gdImageSaveAlpha(im, 1);

	blue = gdTrueColorAlpha(0, 0, 255, 0);
	black = gdTrueColorAlpha(0, 0, 0, 0);
	white = gdTrueColorAlpha(255, 255, 255, 0);
	gdImageFilledRectangle(im, 0, 0, 15, 7, blue);

	for (y = 0; y < 8; y++) {
		for (x = 10; x < 16; x++) {
			gdImageSetPixel(im, x, y, ((x + y) & 1) ? black : white);
		}
	}

	return im;
}

static void test_entropy_selects_high_detail_cover_region(void)
{
	gdImagePtr src, entropy, center;
	gdScaleOptions options = content_options();
	gdRect crop;
	int black = gdTrueColorAlpha(0, 0, 0, 0);
	int white = gdTrueColorAlpha(255, 255, 255, 0);
	int entropy_detail, center_detail;

	src = make_entropy_source();
	gdTestAssert(gdImageEntropyCropRegion(src, 4, 4, &crop));
	gdTestAssert(crop.x >= 8);
	gdTestAssert(crop.width == 8 && crop.height == 8);

	entropy = gdImageScaleWithOptions(src, 4, 4, &options);
	options.strategy = GD_SCALE_STRATEGY_NONE;
	options.gravity = GD_SCALE_GRAVITY_CENTER;
	center = gdImageScaleWithOptions(src, 4, 4, &options);

	assert_dimensions(entropy, 4, 4);
	assert_dimensions(center, 4, 4);
	entropy_detail = count_color(entropy, black) + count_color(entropy, white);
	center_detail = count_color(center, black) + count_color(center, white);
	gdTestAssert(entropy_detail > center_detail);

	gdImageDestroy(center);
	gdImageDestroy(entropy);
	gdImageDestroy(src);
}

static void test_entropy_preserves_alpha(void)
{
	gdImagePtr src, actual;
	gdScaleOptions options = content_options();
	int blue, red, green;
	int x, y, color, red_count, green_count;

	src = gdImageCreateTrueColor(16, 8);
	gdTestAssert(src != NULL);
	gdImageAlphaBlending(src, gdEffectReplace);
	gdImageSaveAlpha(src, 1);
	blue = gdTrueColorAlpha(0, 0, 255, 0);
	red = gdTrueColorAlpha(255, 0, 0, 63);
	green = gdTrueColorAlpha(0, 255, 0, 31);
	gdImageFilledRectangle(src, 0, 0, 15, 7, blue);
	for (y = 0; y < 8; y++) {
		for (x = 10; x < 16; x++) {
			gdImageSetPixel(src, x, y, ((x + y) & 1) ? red : green);
		}
	}

	actual = gdImageScaleWithOptions(src, 4, 4, &options);
	assert_dimensions(actual, 4, 4);
	color = pixel(actual, 2, 0);
	gdTestAssert(color == red || color == green);
	red_count = count_color(actual, red);
	green_count = count_color(actual, green);
	gdTestAssert(red_count + green_count > 0);

	gdImageDestroy(actual);
	gdImageDestroy(src);
}

static void test_entropy_beats_center_when_detail_is_off_center(void)
{
	gdImagePtr src, entropy, center;
	gdScaleOptions options = content_options();
	int blue, black, white, red;
	int x, y, entropy_detail, center_detail;

	src = gdImageCreateTrueColor(48, 16);
	gdTestAssert(src != NULL);
	blue = gdTrueColor(0, 0, 255);
	black = gdTrueColor(0, 0, 0);
	white = gdTrueColor(255, 255, 255);
	red = gdTrueColor(255, 0, 0);
	gdImageFilledRectangle(src, 0, 0, 47, 15, blue);
	for (y = 5; y <= 10; y++) {
		for (x = 2; x <= 7; x++) {
			gdImageSetPixel(src, x, y, ((x + y) & 1) ? red : white);
		}
	}
	for (y = 0; y < 16; y++) {
		for (x = 32; x < 48; x++) {
			gdImageSetPixel(src, x, y, ((x + y) & 1) ? black : white);
		}
	}

	entropy = gdImageScaleWithOptions(src, 16, 16, &options);
	options.strategy = GD_SCALE_STRATEGY_NONE;
	center = gdImageScaleWithOptions(src, 16, 16, &options);
	entropy_detail = count_color(entropy, black) + count_color(entropy, white);
	center_detail = count_color(center, black) + count_color(center, white);
	gdTestAssert(entropy_detail > center_detail);

	gdImageDestroy(center);
	gdImageDestroy(entropy);
	gdImageDestroy(src);
}

static void test_entropy_falls_back_to_gravity_on_flat_images(void)
{
	gdImagePtr src, entropy, center;
	gdScaleOptions options = content_options();

	src = gdImageCreateTrueColor(12, 8);
	gdTestAssert(src != NULL);
	gdImageFilledRectangle(src, 0, 0, 11, 7, gdTrueColor(10, 80, 140));

	entropy = gdImageScaleWithOptions(src, 4, 4, &options);
	options.strategy = GD_SCALE_STRATEGY_NONE;
	options.gravity = GD_SCALE_GRAVITY_CENTER;
	center = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(images_equal(entropy, center));

	gdImageDestroy(center);
	gdImageDestroy(entropy);
	gdImageDestroy(src);
}

static gdImagePtr make_palette_entropy_source(void)
{
	gdImagePtr im;
	int blue, red, black, white;
	int x, y;

	im = gdImageCreateTrueColor(32, 16);
	gdTestAssert(im != NULL);
	blue = gdTrueColor(20, 74, 122);
	red = gdTrueColor(230, 40, 40);
	black = gdTrueColor(0, 0, 0);
	white = gdTrueColor(255, 255, 255);
	gdImageFilledRectangle(im, 0, 0, 31, 15, blue);
	gdImageFilledRectangle(im, 3, 3, 12, 12, red);
	for (y = 0; y < 16; y++) {
		for (x = 22; x < 32; x++) {
			gdImageSetPixel(im, x, y, ((x + y) & 1) ? black : white);
		}
	}

	return im;
}

static gdImagePtr make_indexed_entropy_source(void)
{
	gdImagePtr im;
	int blue, red, black, white;
	int x, y;

	im = gdImageCreate(32, 16);
	gdTestAssert(im != NULL);
	blue = gdImageColorAllocate(im, 20, 74, 122);
	red = gdImageColorAllocate(im, 230, 40, 40);
	black = gdImageColorAllocate(im, 0, 0, 0);
	white = gdImageColorAllocate(im, 255, 255, 255);
	gdImageFilledRectangle(im, 0, 0, 31, 15, blue);
	gdImageFilledRectangle(im, 3, 3, 12, 12, red);
	for (y = 0; y < 16; y++) {
		for (x = 22; x < 32; x++) {
			gdImageSetPixel(im, x, y, ((x + y) & 1) ? black : white);
		}
	}

	return im;
}

static void test_entropy_palette_and_truecolor_equivalent(void)
{
	gdImagePtr truecolor, palette, from_truecolor, from_palette;
	gdScaleOptions options = content_options();

	truecolor = make_palette_entropy_source();
	palette = make_indexed_entropy_source();
	gdTestAssert(gdImageTrueColor(truecolor));
	gdTestAssert(!gdImageTrueColor(palette));

	from_truecolor = gdImageScaleWithOptions(truecolor, 8, 8, &options);
	from_palette = gdImageScaleWithOptions(palette, 8, 8, &options);
	assert_dimensions(from_truecolor, 8, 8);
	assert_dimensions(from_palette, 8, 8);
	gdTestAssert(gdImageTrueColor(from_truecolor));
	gdTestAssert(gdImageTrueColor(from_palette));
	gdTestAssert(gdMaxPixelDiff(from_truecolor, from_palette) <= 2);

	gdImageDestroy(from_palette);
	gdImageDestroy(from_truecolor);
	gdImageDestroy(palette);
	gdImageDestroy(truecolor);
}

static void test_symmetric_competing_detail_regions_are_deterministic(void)
{
	gdImagePtr src, entropy, west, east;
	gdScaleOptions options = content_options();
	int blue, black, white;
	int x, y, entropy_detail, west_detail, east_detail;

	src = gdImageCreateTrueColor(48, 16);
	gdTestAssert(src != NULL);
	blue = gdTrueColor(0, 0, 255);
	black = gdTrueColor(0, 0, 0);
	white = gdTrueColor(255, 255, 255);
	gdImageFilledRectangle(src, 0, 0, 47, 15, blue);
	for (y = 0; y < 16; y++) {
		for (x = 0; x < 16; x++) {
			gdImageSetPixel(src, x, y, ((x + y) & 1) ? black : white);
		}
		for (x = 32; x < 48; x++) {
			gdImageSetPixel(src, x, y, ((x + y) & 1) ? black : white);
		}
	}

	entropy = gdImageScaleWithOptions(src, 16, 16, &options);
	options.strategy = GD_SCALE_STRATEGY_NONE;
	options.gravity = GD_SCALE_GRAVITY_WEST;
	west = gdImageScaleWithOptions(src, 16, 16, &options);
	options.gravity = GD_SCALE_GRAVITY_EAST;
	east = gdImageScaleWithOptions(src, 16, 16, &options);

	entropy_detail = count_color(entropy, black) + count_color(entropy, white);
	west_detail = count_color(west, black) + count_color(west, white);
	east_detail = count_color(east, black) + count_color(east, white);
	gdTestAssert(entropy_detail == west_detail || entropy_detail == east_detail);
	gdTestAssert(entropy_detail == 256);

	gdImageDestroy(east);
	gdImageDestroy(west);
	gdImageDestroy(entropy);
	gdImageDestroy(src);
}

#ifdef HAVE_LIBJPEG
static gdImagePtr image_from_jpeg_file(const char *path)
{
	FILE *fp;
	gdImagePtr im;

	fp = gdTestFileOpen(path);
	if (fp == NULL) {
		return NULL;
	}

	im = gdImageCreateFromJpeg(fp);
	fclose(fp);
	return im;
}

static void assert_jpeg_perceptual(gdImagePtr actual, const char *expected_file,
								   double threshold,
								   unsigned int max_pixels_changed)
{
	gdImagePtr expected, diff = NULL;
	gdImagePerceptualDiffResult result;

	expected = image_from_jpeg_file(expected_file);
	gdTestAssert(expected != NULL);
	gdTestAssert(gdImageSX(expected) == gdImageSX(actual));
	gdTestAssert(gdImageSY(expected) == gdImageSY(actual));
	gdTestAssert(gdImagePerceptualDiff(expected, actual, threshold, NULL, &diff,
									   &result));
	gdTestAssertMsg(result.pixels_changed < max_pixels_changed,
					"perceptual diff changed %u pixels, allowed less than %u\n",
					result.pixels_changed, max_pixels_changed);

	if (diff != NULL) {
		gdImageDestroy(diff);
	}
	gdImageDestroy(expected);
}

static void test_photo_fixtures(void)
{
	gdImagePtr src, entropy, attention;
	gdScaleOptions options = content_options();

	src = image_from_jpeg_file("gdimagescale/gd_image_scale_entropy_portrait_right_side.jpg");
	gdTestAssert(src != NULL);
	gdTestAssert(gdImageSX(src) == 240);
	gdTestAssert(gdImageSY(src) == 160);

	options.interpolation = GD_BILINEAR_FIXED;
	options.strategy = GD_SCALE_STRATEGY_ENTROPY;
	entropy = gdImageScaleWithOptions(src, 120, 180, &options);
	assert_dimensions(entropy, 120, 180);
	assert_jpeg_perceptual(entropy,
						   "gdimagescale/gd_image_scale_entropy_portrait_right_side_expected.jpg",
						   0.20, 2200);

	options.strategy = GD_SCALE_STRATEGY_ATTENTION;
	attention = gdImageScaleWithOptions(src, 120, 180, &options);
	assert_dimensions(attention, 120, 180);
	assert_jpeg_perceptual(attention,
						   "gdimagescale/gd_image_scale_attention_portrait_right_side_expected.jpg",
						   0.10, 1500);

	gdImageDestroy(attention);
	gdImageDestroy(entropy);
	gdImageDestroy(src);
}
#endif

int main(void)
{
	test_entropy_selects_high_detail_cover_region();
	test_entropy_preserves_alpha();
	test_entropy_beats_center_when_detail_is_off_center();
	test_entropy_falls_back_to_gravity_on_flat_images();
	test_entropy_palette_and_truecolor_equivalent();
	test_symmetric_competing_detail_regions_are_deterministic();
#ifdef HAVE_LIBJPEG
	test_photo_fixtures();
#endif

	return gdNumFailures();
}
