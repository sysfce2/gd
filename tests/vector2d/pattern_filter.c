#include "gd_vector2d.h"
#include "gdtest.h"

#include <stdint.h>

static gdImagePtr make_argb_source(int w, int h)
{
	gdImagePtr im = gdImageCreateTrueColor(w, h);
	gdTestAssert(im != NULL);
	gdImageAlphaBlending(im, 0);
	gdImageSaveAlpha(im, 1);
	return im;
}

static gdImagePtr render_scaled(gdImagePtr source, int w, int h, double sx, double sy,
		gdPatternFilter filter)
{
	gdImagePtr target = gdImageCreateTrueColor(w, h);
	gdContextPtr ctx;

	gdTestAssert(target != NULL);
	gdImageAlphaBlending(target, 0);
	gdImageSaveAlpha(target, 1);
	ctx = gdContextCreateForImage(target);
	gdTestAssert(ctx != NULL);
	gdContextSetPatternFilter(ctx, filter);
	gdContextSetSourceImage(ctx, source, 0, 0);
	gdContextScale(ctx, sx, sy);
	gdContextPaint(ctx);
	gdContextDestroy(ctx);
	return target;
}

static void assert_premul_valid(gdImagePtr im)
{
	for (int y = 0; y < gdImageSY(im); y++) {
		for (int x = 0; x < gdImageSX(im); x++) {
			int p = gdImageGetTrueColorPixel(im, x, y);
			int a = 255 - gdTrueColorGetAlpha(p) * 2;
			int r = gdTrueColorGetRed(p);
			int g = gdTrueColorGetGreen(p);
			int b = gdTrueColorGetBlue(p);
			if (a < 0)
				a = 0;
			gdTestAssert(r <= a + 1);
			gdTestAssert(g <= a + 1);
			gdTestAssert(b <= a + 1);
		}
	}
}

static void test_filter_api_state(void)
{
	gdImagePtr im = make_argb_source(1, 1);
	gdContextPtr ctx = gdContextCreateForImage(im);
	gdPathPatternPtr pattern = gdPathPatternCreateForImage(im);

	gdTestAssert(ctx != NULL);
	gdTestAssert(pattern != NULL);
	gdTestAssert(gdContextGetPatternFilter(ctx) == GD_PATTERN_FILTER_GOOD);
	gdContextSetPatternFilter(ctx, GD_PATTERN_FILTER_FAST);
	gdTestAssert(gdContextGetPatternFilter(ctx) == GD_PATTERN_FILTER_FAST);
	gdTestAssert(gdContextSave(ctx) == 1);
	gdContextSetPatternFilter(ctx, GD_PATTERN_FILTER_BEST);
	gdTestAssert(gdContextGetPatternFilter(ctx) == GD_PATTERN_FILTER_BEST);
	gdTestAssert(gdContextRestore(ctx) == 1);
	gdTestAssert(gdContextGetPatternFilter(ctx) == GD_PATTERN_FILTER_FAST);

	gdTestAssert(gdPathPatternGetFilter(pattern) == GD_PATTERN_FILTER_GOOD);
	gdPathPatternSetFilter(pattern, GD_PATTERN_FILTER_BEST);
	gdTestAssert(gdPathPatternGetFilter(pattern) == GD_PATTERN_FILTER_BEST);

	gdPathPatternDestroy(pattern);
	gdContextDestroy(ctx);
	gdImageDestroy(im);
}

static void test_good_uses_mipmap_for_minification(void)
{
	gdImagePtr source = make_argb_source(2, 2);
	gdImagePtr fast;
	gdImagePtr good;
	int fp, gp;

	gdImageSetPixel(source, 0, 0, gdTrueColorAlpha(0, 0, 0, 0));
	gdImageSetPixel(source, 1, 0, gdTrueColorAlpha(255, 0, 0, 0));
	gdImageSetPixel(source, 0, 1, gdTrueColorAlpha(0, 255, 0, 0));
	gdImageSetPixel(source, 1, 1, gdTrueColorAlpha(0, 0, 255, 0));

	fast = render_scaled(source, 1, 1, 0.5, 0.5, GD_PATTERN_FILTER_FAST);
	good = render_scaled(source, 1, 1, 0.5, 0.5, GD_PATTERN_FILTER_GOOD);
	fp = gdImageGetTrueColorPixel(fast, 0, 0);
	gp = gdImageGetTrueColorPixel(good, 0, 0);

	gdTestAssert(gdTrueColorGetBlue(fp) > 32);
	gdTestAssert(gdTrueColorGetRed(gp) > gdTrueColorGetRed(fp));
	gdTestAssert(gdTrueColorGetGreen(gp) > gdTrueColorGetGreen(fp));
	gdTestAssert(gdTrueColorGetBlue(gp) < gdTrueColorGetBlue(fp));

	gdImageDestroy(fast);
	gdImageDestroy(good);
	gdImageDestroy(source);
}

static void test_best_preserves_premultiplied_alpha(void)
{
	gdImagePtr source = make_argb_source(3, 3);
	gdImagePtr result;

	for (int y = 0; y < 3; y++) {
		for (int x = 0; x < 3; x++) {
			int alpha = (x == 1 && y == 1) ? 0 : 127;
			gdImageSetPixel(source, x, y, gdTrueColorAlpha(255, 255, 255, alpha));
		}
	}

	result = render_scaled(source, 12, 12, 4.0, 4.0, GD_PATTERN_FILTER_BEST);
	assert_premul_valid(result);
	gdImageDestroy(result);
	gdImageDestroy(source);
}

int main(void)
{
	test_filter_api_state();
	test_good_uses_mipmap_for_minification();
	test_best_preserves_premultiplied_alpha();
	return gdNumFailures();
}
