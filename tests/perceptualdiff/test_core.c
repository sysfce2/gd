#include "gd.h"
#include "gdtest.h"
#include <stdio.h>

/* Helper: create a simple truecolor image filled with a solid color */
static gdImagePtr make_solid(int w, int h, int r, int g, int b, int a) {
	gdImagePtr im = gdImageCreateTrueColor(w, h);
	int color = gdTrueColorAlpha(r, g, b, a);
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			gdImageSetPixel(im, x, y, color);
		}
	}
	return im;
}

/* Test 1: identical images produce zero diff */
static void test_perceptual_diff_identical() {
	gdImagePtr img1 = make_solid(64, 64, 128, 64, 200, 0);
	gdImagePtr img2 = make_solid(64, 64, 128, 64, 200, 0);
	gdImagePtr diff = NULL;
	gdImagePerceptualDiffResult result;

	gdTestAssert(gdImagePerceptualDiff(img1, img2, 0.1, NULL, &diff, &result));

	gdTestAssertMsg(result.pixels_changed == 0,
					"Expected 0 pixels changed, but got %d",
					result.pixels_changed);
	gdTestAssert(gdTrueColorGetRed(gdImageGetTrueColorPixel(diff, 0, 0)) == 128);
	gdTestAssert(gdTrueColorGetAlpha(gdImageGetTrueColorPixel(diff, 0, 0)) == 102);
	gdTestAssert(result.maximum_delta == 0.0);

	gdImageDestroy(img1);
	gdImageDestroy(img2);
	gdImageDestroy(diff);
}

/* Test 2: completely different images (black vs white) exceed threshold */
static void test_perceptual_diff_black_vs_white() {
	gdImagePtr img1 = make_solid(64, 64, 0, 0, 0, 0);		/* black opaque */
	gdImagePtr img2 = make_solid(64, 64, 255, 255, 255, 0); /* white opaque */
	gdImagePtr diff = NULL;
	gdImagePerceptualDiffResult result;

	gdTestAssert(gdImagePerceptualDiff(img1, img2, 0.1, NULL, &diff, &result));

	gdTestAssertMsg(result.pixels_changed == 64 * 64,
					"Expected %d pixels changed, but got %d", 64 * 64,
					result.pixels_changed);
	gdTestAssert(result.maximum_delta > 0.1 && result.maximum_delta <= 1.0);

	gdImageDestroy(img1);
	gdImageDestroy(img2);
	FILE *fp = fopen("perceptualdiff_black_vs_white_diff.png", "wb");
	if (fp) {
		gdImagePng(diff, fp);
		fclose(fp);
	}
	gdImageDestroy(diff);
}

/* Test 3: subtle color difference below threshold produces no diff */
static void test_perceptual_diff_subtle_below_threshold() {
	/* two colors that differ by 1 in each channel — well below any sane
	 * threshold */
	gdImagePtr img1 = make_solid(64, 64, 100, 100, 100, 0);
	gdImagePtr img2 = make_solid(64, 64, 101, 101, 101, 0);
	gdImagePerceptualDiffResult result;

	gdTestAssert(gdImagePerceptualDiff(img1, img2, 0.1, NULL, NULL, &result));

	gdTestAssertMsg(result.pixels_changed == 0,
					"Expected 0 pixels changed, but got %d",
					result.pixels_changed);

	gdImageDestroy(img1);
	gdImageDestroy(img2);
}

/* Test 4: alpha-only difference — both fully transparent, should not count */
static void test_perceptual_diff_both_transparent() {
	/* same RGB, both fully transparent (GD alpha 127) */
	gdImagePtr img1 = make_solid(64, 64, 255, 0, 0, 127);
	gdImagePtr img2 = make_solid(64, 64, 0, 255, 0, 127);
	gdImagePerceptualDiffResult result;

	gdTestAssert(gdImagePerceptualDiff(img1, img2, 0.1, NULL, NULL, &result));

	gdTestAssertMsg(result.pixels_changed == 0,
					"Expected 0 pixels changed, but got %d",
					result.pixels_changed);

	gdImageDestroy(img1);
	gdImageDestroy(img2);
}

/* Test 5: significant alpha difference should count as diff */
static void test_perceptual_diff_alpha_delta() {
	/* same RGB, one opaque, one fully transparent */
	gdImagePtr img1 = make_solid(32, 32, 200, 100, 50, 0); /* opaque */
	gdImagePtr img2 =
		make_solid(32, 32, 200, 100, 50, 127); /* fully transparent */
	gdImagePerceptualDiffResult result;

	gdTestAssert(gdImagePerceptualDiff(img1, img2, 0.1, NULL, NULL, &result));

	gdTestAssertMsg(result.pixels_changed == 32 * 32,
					"Expected %d pixels changed, but got %d", 32 * 32,
					result.pixels_changed);

	gdImageDestroy(img1);
	gdImageDestroy(img2);
}

/* Test 6: diff image output — changed pixels are red, unchanged are faded */
static void test_perceptual_diff_image_output() {
	gdImagePtr img1 = make_solid(4, 4, 0, 0, 0, 0);
	gdImagePtr img2 = make_solid(4, 4, 255, 255, 255, 0);
	gdImagePtr diff = NULL;
	gdImagePerceptualDiffResult result;

	gdTestAssert(gdImagePerceptualDiff(img1, img2, 0.1, NULL, &diff, &result));

	/* every pixel should be red (255,0,0 opaque) in the diff image */
	int x, y;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			int p = gdImageGetTrueColorPixel(diff, x, y);
			gdTestAssertMsg(gdTrueColorGetRed(p) == 255,
							"Expected red 255, but got %d",
							gdTrueColorGetRed(p));
			gdTestAssertMsg(gdTrueColorGetGreen(p) == 0,
							"Expected green 0, but got %d",
							gdTrueColorGetGreen(p));
			gdTestAssertMsg(gdTrueColorGetBlue(p) == 0,
							"Expected blue 0, but got %d",
							gdTrueColorGetBlue(p));
			gdTestAssertMsg(gdTrueColorGetAlpha(p) == 0,
							"Expected alpha 0, but got %d",
							gdTrueColorGetAlpha(p));
		}
	}

	gdImageDestroy(img1);
	gdImageDestroy(img2);
	FILE *fp = fopen("test_perceptual_diff_image_output.png", "wb");
	if (fp) {
		gdImagePng(diff, fp);
		fclose(fp);
	}
	gdImageDestroy(diff);
}

static void test_perceptual_diff_mask_and_custom_color(void) {
	gdImagePtr img1 = make_solid(2, 1, 0, 0, 0, 0);
	gdImagePtr img2 = make_solid(2, 1, 0, 0, 0, 0);
	gdImagePtr diff = NULL;
	gdImagePerceptualDiffOptions options = {
		GD_IMAGE_DIFF_MASK, gdTrueColorAlpha(12, 34, 56, 7)};
	gdImagePerceptualDiffResult result;
	int changed;
	int unchanged;

	gdImageSetPixel(img2, 0, 0, gdTrueColorAlpha(255, 255, 255, 0));
	gdTestAssert(gdImagePerceptualDiff(img1, img2, 0.1, &options, &diff,
										&result));
	changed = gdImageGetTrueColorPixel(diff, 0, 0);
	unchanged = gdImageGetTrueColorPixel(diff, 1, 0);
	gdTestAssert(result.pixels_changed == 1);
	gdTestAssert(changed == options.highlight_color);
	gdTestAssert(gdTrueColorGetAlpha(unchanged) == 127);

	gdImageDestroy(diff);
	gdImageDestroy(img2);
	gdImageDestroy(img1);
}

static void test_perceptual_diff_invalid_arguments(void) {
	gdImagePtr img1 = make_solid(2, 2, 0, 0, 0, 0);
	gdImagePtr img2 = make_solid(3, 2, 0, 0, 0, 0);
	gdImagePtr diff = img1;
	gdImagePerceptualDiffOptions options = {GD_IMAGE_DIFF_MASK + 1, 0};
	gdImagePerceptualDiffOptions no_image = {GD_IMAGE_DIFF_NONE, 0};
	gdImagePerceptualDiffResult result = {1, 1.0};

	gdTestAssert(!gdImagePerceptualDiff(NULL, img1, 0.1, NULL, &diff, &result));
	gdTestAssert(diff == NULL && result.pixels_changed == 0 &&
				 result.maximum_delta == 0.0);
	gdTestAssert(!gdImagePerceptualDiff(img1, img2, 0.1, NULL, NULL, &result));
	gdTestAssert(!gdImagePerceptualDiff(img1, img1, -0.1, NULL, NULL, &result));
	gdTestAssert(!gdImagePerceptualDiff(img1, img1, 1.1, NULL, NULL, &result));
	gdTestAssert(
		!gdImagePerceptualDiff(img1, img1, 0.1, &options, NULL, &result));
	gdTestAssert(!gdImagePerceptualDiff(img1, img1, 0.1, NULL, NULL, NULL));
	diff = img1;
	gdTestAssert(gdImagePerceptualDiff(img1, img1, 0.1, &no_image, &diff, &result));
	gdTestAssert(diff == NULL);

	gdImageDestroy(img2);
	gdImageDestroy(img1);
}

int main() {
	test_perceptual_diff_identical();
	test_perceptual_diff_black_vs_white();
	test_perceptual_diff_subtle_below_threshold();
	test_perceptual_diff_both_transparent();
	test_perceptual_diff_alpha_delta();
	test_perceptual_diff_image_output();
	test_perceptual_diff_mask_and_custom_color();
	test_perceptual_diff_invalid_arguments();
	return gdNumFailures();
}
