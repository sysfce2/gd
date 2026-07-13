#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gd.h"
#include "gdtest.h"

static gdScaleOptions scale_options(void)
{
	gdScaleOptions options;

	options.fit = GD_SCALE_FIT_COVER;
	options.gravity = GD_SCALE_GRAVITY_CENTER;
	options.strategy = GD_SCALE_STRATEGY_NONE;
	options.background_color = gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent);
	options.interpolation = GD_SCALE_INTERPOLATION_AUTO;

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

static gdImagePtr make_pattern_source(int width, int height)
{
	gdImagePtr im;
	int x, y;

	im = gdImageCreateTrueColor(width, height);
	gdTestAssert(im != NULL);
	gdImageAlphaBlending(im, gdEffectReplace);
	gdImageSaveAlpha(im, 1);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			gdImageSetPixel(im, x, y,
							gdTrueColorAlpha((x * 25) & 255,
											 (y * 45) & 255,
											 (80 + x + y) & 255, 0));
		}
	}

	return im;
}

static gdImagePtr make_default_interpolation_source(int width, int height)
{
	gdImagePtr im;
	int x, y;

	im = gdImageCreateTrueColor(width, height);
	gdTestAssert(im != NULL);
	gdImageAlphaBlending(im, gdEffectReplace);
	gdImageSaveAlpha(im, 1);
	gdImageFilledRectangle(im, 0, 0, width - 1, height - 1,
						   gdTrueColorAlpha(255, 255, 255, 0));

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			gdImageSetPixel(im, x, y,
							gdTrueColorAlpha((x * 17 + y * 3) % 256,
											 (x * 5 + y * 19) % 256,
											 (x * 11 + y * 7) % 256, 0));
		}
	}

	return im;
}

static gdImagePtr make_alpha_source(int white_alpha)
{
	gdImagePtr im;

	im = gdImageCreateTrueColor(2, 2);
	gdTestAssert(im != NULL);
	gdImageAlphaBlending(im, gdEffectReplace);
	gdImageSaveAlpha(im, 1);
	gdImageSetPixel(im, 0, 0, gdTrueColorAlpha(255, 0, 0, 63));
	gdImageSetPixel(im, 1, 0, gdTrueColorAlpha(0, 255, 0, 31));
	gdImageSetPixel(im, 0, 1, gdTrueColorAlpha(0, 0, 255, 95));
	gdImageSetPixel(im, 1, 1, gdTrueColorAlpha(255, 255, 255, white_alpha));

	return im;
}

static void test_fill_equals_gdimagescale(void)
{
	gdImagePtr src, expected, actual;
	gdScaleOptions options = scale_options();

	src = make_pattern_source(8, 4);
	options.fit = GD_SCALE_FIT_FILL;
	options.interpolation = GD_BILINEAR_FIXED;

	gdImageSetInterpolationMethod(src, GD_BILINEAR_FIXED);
	expected = gdImageScale(src, 4, 4);
	actual = gdImageScaleWithOptions(src, 4, 4, &options);

	gdTestAssert(images_equal(expected, actual));

	gdImageDestroy(actual);
	gdImageDestroy(expected);
	gdImageDestroy(src);
}

static void test_contain_background_and_transparent_source(void)
{
	gdImagePtr src, actual;
	gdScaleOptions options = scale_options();
	int transparent, blue, green;

	src = gdImageCreateTrueColor(4, 2);
	gdTestAssert(src != NULL);
	gdImageAlphaBlending(src, gdEffectReplace);
	gdImageSaveAlpha(src, 1);
	transparent = gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent);
	blue = gdTrueColorAlpha(20, 100, 220, 0);
	green = gdTrueColorAlpha(0, 255, 0, 50);
	gdImageFilledRectangle(src, 0, 0, 3, 1, transparent);
	gdImageFilledRectangle(src, 1, 0, 3, 1, blue);

	options.fit = GD_SCALE_FIT_CONTAIN;
	options.interpolation = GD_NEAREST_NEIGHBOUR;
	actual = gdImageScaleWithOptions(src, 8, 8, &options);
	assert_dimensions(actual, 8, 8);
	gdTestAssert(pixel(actual, 0, 0) == gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent));
	gdImageDestroy(actual);

	options.background_color = green;
	actual = gdImageScaleWithOptions(src, 8, 8, &options);
	assert_dimensions(actual, 8, 8);
	gdTestAssert(pixel(actual, 0, 0) == green);
	gdTestAssert(pixel(actual, 0, 2) == green);
	gdTestAssert(pixel(actual, 2, 2) == blue);

	gdImageDestroy(actual);
	gdImageDestroy(src);
}

static gdImagePtr make_row_source(void)
{
	gdImagePtr im = gdImageCreateTrueColor(4, 4);
	int colors[4] = {
		gdTrueColor(255, 0, 0),
		gdTrueColor(0, 255, 0),
		gdTrueColor(0, 0, 255),
		gdTrueColor(255, 255, 255)
	};
	int y;

	gdTestAssert(im != NULL);
	for (y = 0; y < 4; y++) {
		gdImageFilledRectangle(im, 0, y, 3, y, colors[y]);
	}

	return im;
}

static gdImagePtr make_column_source(void)
{
	gdImagePtr im = gdImageCreateTrueColor(4, 4);
	int colors[4] = {
		gdTrueColor(255, 0, 0),
		gdTrueColor(0, 255, 0),
		gdTrueColor(0, 0, 255),
		gdTrueColor(255, 255, 255)
	};
	int x;

	gdTestAssert(im != NULL);
	for (x = 0; x < 4; x++) {
		gdImageFilledRectangle(im, x, 0, x, 3, colors[x]);
	}

	return im;
}

static void test_cover_gravity(void)
{
	gdScaleGravity vertical[9] = {
		GD_SCALE_GRAVITY_NORTHWEST, GD_SCALE_GRAVITY_NORTH,
		GD_SCALE_GRAVITY_NORTHEAST, GD_SCALE_GRAVITY_WEST,
		GD_SCALE_GRAVITY_CENTER, GD_SCALE_GRAVITY_EAST,
		GD_SCALE_GRAVITY_SOUTHWEST, GD_SCALE_GRAVITY_SOUTH,
		GD_SCALE_GRAVITY_SOUTHEAST
	};
	int expected_vertical[9][2] = {
		{0, 1}, {0, 1}, {0, 1}, {1, 2}, {1, 2}, {1, 2}, {2, 3}, {2, 3}, {2, 3}
	};
	int expected_horizontal[9][2] = {
		{0, 1}, {1, 2}, {2, 3}, {0, 1}, {1, 2}, {2, 3}, {0, 1}, {1, 2}, {2, 3}
	};
	int colors[4] = {
		gdTrueColor(255, 0, 0), gdTrueColor(0, 255, 0),
		gdTrueColor(0, 0, 255), gdTrueColor(255, 255, 255)
	};
	gdImagePtr rows, columns, actual;
	gdScaleOptions options = scale_options();
	int i;

	rows = make_row_source();
	columns = make_column_source();
	options.fit = GD_SCALE_FIT_COVER;
	options.interpolation = GD_NEAREST_NEIGHBOUR;

	for (i = 0; i < 9; i++) {
		options.gravity = vertical[i];
		actual = gdImageScaleWithOptions(rows, 4, 2, &options);
		assert_dimensions(actual, 4, 2);
		gdTestAssert(pixel(actual, 0, 0) == colors[expected_vertical[i][0]]);
		gdTestAssert(pixel(actual, 0, 1) == colors[expected_vertical[i][1]]);
		gdImageDestroy(actual);

		actual = gdImageScaleWithOptions(columns, 2, 4, &options);
		assert_dimensions(actual, 2, 4);
		gdTestAssert(pixel(actual, 0, 0) == colors[expected_horizontal[i][0]]);
		gdTestAssert(pixel(actual, 1, 0) == colors[expected_horizontal[i][1]]);
		gdImageDestroy(actual);
	}

	gdImageDestroy(columns);
	gdImageDestroy(rows);
}

static void test_inside_outside_dimensions(void)
{
	gdImagePtr wide, tall, square, actual;
	gdScaleOptions options = scale_options();

	wide = gdImageCreateTrueColor(400, 200);
	tall = gdImageCreateTrueColor(200, 400);
	square = gdImageCreateTrueColor(100, 100);
	gdTestAssert(wide != NULL && tall != NULL && square != NULL);

	options.fit = GD_SCALE_FIT_INSIDE;
	actual = gdImageScaleWithOptions(wide, 100, 100, &options);
	assert_dimensions(actual, 100, 50);
	gdImageDestroy(actual);
	actual = gdImageScaleWithOptions(tall, 100, 100, &options);
	assert_dimensions(actual, 50, 100);
	gdImageDestroy(actual);
	actual = gdImageScaleWithOptions(square, 50, 50, &options);
	assert_dimensions(actual, 50, 50);
	gdImageDestroy(actual);

	options.fit = GD_SCALE_FIT_OUTSIDE;
	actual = gdImageScaleWithOptions(wide, 100, 100, &options);
	assert_dimensions(actual, 200, 100);
	gdImageDestroy(actual);
	actual = gdImageScaleWithOptions(tall, 100, 100, &options);
	assert_dimensions(actual, 100, 200);
	gdImageDestroy(actual);
	actual = gdImageScaleWithOptions(square, 50, 50, &options);
	assert_dimensions(actual, 50, 50);
	gdImageDestroy(actual);

	gdImageDestroy(square);
	gdImageDestroy(tall);
	gdImageDestroy(wide);
}

static void test_inside_outside_alpha(void)
{
	gdImagePtr src, expected, actual;
	gdScaleOptions options = scale_options();

	src = make_alpha_source(10);
	options.interpolation = GD_NEAREST_NEIGHBOUR;
	options.background_color = gdTrueColorAlpha(255, 255, 0, 0);

	options.fit = GD_SCALE_FIT_INSIDE;
	actual = gdImageScaleWithOptions(src, 4, 6, &options);
	gdImageSetInterpolationMethod(src, GD_NEAREST_NEIGHBOUR);
	expected = gdImageScale(src, 4, 4);
	assert_dimensions(actual, 4, 4);
	gdTestAssert(images_equal(expected, actual));
	gdTestAssert(pixel(actual, 0, 0) == gdTrueColorAlpha(255, 0, 0, 63));
	gdTestAssert(pixel(actual, 3, 0) == gdTrueColorAlpha(0, 255, 0, 31));
	gdImageDestroy(actual);
	gdImageDestroy(expected);

	options.fit = GD_SCALE_FIT_OUTSIDE;
	actual = gdImageScaleWithOptions(src, 4, 6, &options);
	gdImageSetInterpolationMethod(src, GD_NEAREST_NEIGHBOUR);
	expected = gdImageScale(src, 6, 6);
	assert_dimensions(actual, 6, 6);
	gdTestAssert(images_equal(expected, actual));
	gdTestAssert(pixel(actual, 0, 0) == gdTrueColorAlpha(255, 0, 0, 63));
	gdTestAssert(pixel(actual, 5, 0) == gdTrueColorAlpha(0, 255, 0, 31));
	gdImageDestroy(actual);
	gdImageDestroy(expected);

	gdImageDestroy(src);
}

static void test_alpha_fit_modes_and_source_state(void)
{
	gdImagePtr src, expected, actual;
	gdScaleOptions options = scale_options();
	int alpha_blending_flag, save_alpha_flag;

	src = make_alpha_source(0);
	alpha_blending_flag = src->alphaBlendingFlag;
	save_alpha_flag = src->saveAlphaFlag;
	options.interpolation = GD_NEAREST_NEIGHBOUR;
	options.background_color = gdTrueColorAlpha(0, 255, 0, 50);

	options.fit = GD_SCALE_FIT_FILL;
	actual = gdImageScaleWithOptions(src, 4, 4, &options);
	gdImageSetInterpolationMethod(src, GD_NEAREST_NEIGHBOUR);
	expected = gdImageScale(src, 4, 4);
	assert_dimensions(actual, 4, 4);
	gdTestAssert(images_equal(expected, actual));
	gdTestAssert(pixel(actual, 0, 0) == gdTrueColorAlpha(255, 0, 0, 63));
	gdTestAssert(pixel(actual, 3, 0) == gdTrueColorAlpha(0, 255, 0, 31));
	gdImageDestroy(actual);
	gdImageDestroy(expected);

	options.fit = GD_SCALE_FIT_CONTAIN;
	actual = gdImageScaleWithOptions(src, 4, 6, &options);
	assert_dimensions(actual, 4, 6);
	gdTestAssert(pixel(actual, 0, 0) == options.background_color);
	gdTestAssert(pixel(actual, 0, 1) == gdTrueColorAlpha(255, 0, 0, 63));
	gdTestAssert(pixel(actual, 3, 1) == gdTrueColorAlpha(0, 255, 0, 31));
	gdImageDestroy(actual);

	options.fit = GD_SCALE_FIT_COVER;
	options.gravity = GD_SCALE_GRAVITY_NORTH;
	actual = gdImageScaleWithOptions(src, 6, 4, &options);
	assert_dimensions(actual, 6, 4);
	gdTestAssert(pixel(actual, 0, 0) == gdTrueColorAlpha(255, 0, 0, 63));
	gdTestAssert(pixel(actual, 5, 0) == gdTrueColorAlpha(0, 255, 0, 31));
	gdImageDestroy(actual);

	gdTestAssert(src->alphaBlendingFlag == alpha_blending_flag);
	gdTestAssert(src->saveAlphaFlag == save_alpha_flag);
	gdTestAssert(pixel(src, 0, 0) == gdTrueColorAlpha(255, 0, 0, 63));

	gdImageDestroy(src);
}

static void test_default_interpolation(void)
{
	gdImagePtr down_src, down_auto, down_lanczos, down_catmull;
	gdImagePtr up_src, up_auto, up_catmull, up_bicubic, up_default;
	gdScaleOptions options = scale_options();

	options.fit = GD_SCALE_FIT_FILL;

	down_src = make_default_interpolation_source(80, 50);
	down_auto = gdImageScaleWithOptions(down_src, 20, 10, &options);
	options.interpolation = GD_LANCZOS3;
	down_lanczos = gdImageScaleWithOptions(down_src, 20, 10, &options);
	options.interpolation = GD_CATMULLROM;
	down_catmull = gdImageScaleWithOptions(down_src, 20, 10, &options);
	gdTestAssert(images_equal(down_auto, down_lanczos));
	gdTestAssert(!images_equal(down_auto, down_catmull));
	gdImageDestroy(down_catmull);
	gdImageDestroy(down_lanczos);
	gdImageDestroy(down_auto);
	gdImageDestroy(down_src);

	up_src = make_default_interpolation_source(10, 8);
	options.interpolation = GD_SCALE_INTERPOLATION_AUTO;
	up_auto = gdImageScaleWithOptions(up_src, 40, 32, &options);
	options.interpolation = GD_CATMULLROM;
	up_catmull = gdImageScaleWithOptions(up_src, 40, 32, &options);
	options.interpolation = GD_BICUBIC_FIXED;
	up_bicubic = gdImageScaleWithOptions(up_src, 40, 32, &options);
	options.interpolation = GD_DEFAULT;
	up_default = gdImageScaleWithOptions(up_src, 40, 32, &options);
	gdTestAssert(images_equal(up_auto, up_catmull));
	gdTestAssert(!images_equal(up_auto, up_bicubic));
	gdTestAssert(!images_equal(up_auto, up_default));

	gdImageDestroy(up_default);
	gdImageDestroy(up_bicubic);
	gdImageDestroy(up_catmull);
	gdImageDestroy(up_auto);
	gdImageDestroy(up_src);
}

static void test_validation_errors(void)
{
	gdImagePtr src, result;
	gdScaleOptions options = scale_options();

	src = gdImageCreateTrueColor(4, 4);
	gdTestAssert(src != NULL);

	result = gdImageScaleWithOptions(src, 0, 1, &options);
	gdTestAssert(result == NULL);
	result = gdImageScaleWithOptions(src, 1, 0, &options);
	gdTestAssert(result == NULL);

	options.interpolation = -100;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);
	options.interpolation = 100;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);
	options.interpolation = GD_SCALE_INTERPOLATION_AUTO;

	options.strategy = GD_SCALE_STRATEGY_ENTROPY;
	options.fit = GD_SCALE_FIT_CONTAIN;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);
	options.fit = GD_SCALE_FIT_FILL;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);
	options.strategy = GD_SCALE_STRATEGY_ATTENTION;
	options.fit = GD_SCALE_FIT_INSIDE;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);
	options.fit = GD_SCALE_FIT_OUTSIDE;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);
	options.fit = GD_SCALE_FIT_COVER;
	options.strategy = (gdScaleStrategy)100;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);

	options = scale_options();
	options.fit = (gdScaleFit)100;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);
	options = scale_options();
	options.gravity = (gdScaleGravity)100;
	result = gdImageScaleWithOptions(src, 4, 4, &options);
	gdTestAssert(result == NULL);

	gdImageDestroy(src);
}

int main(void)
{
	test_fill_equals_gdimagescale();
	test_contain_background_and_transparent_source();
	test_cover_gravity();
	test_inside_outside_dimensions();
	test_inside_outside_alpha();
	test_alpha_fit_modes_and_source_state();
	test_default_interpolation();
	test_validation_errors();

	return gdNumFailures();
}
