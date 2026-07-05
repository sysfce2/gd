#include "gd.h"
#include "gdtest.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PI 3.141592
#define DELTA (PI / 8)
#define PERCEPTUAL_THRESHOLD 0.05
#define MAX_PERCEPTUAL_PIXELS_CHANGED 1500

int main() {
	char *path = NULL;
	gdImagePtr ref = NULL;
	gdImagePtr im = NULL;
	int black;
	double cos_t, sin_t;
	int x, y, temp;
	int i;
	int brect[8];
	int error = 0;
	gdImagePerceptualDiffResult result;

	/* disable subpixel hinting */
	putenv("FREETYPE_PROPERTIES=truetype:interpreter-version=35");

	path = gdTestFilePath("freetype/DejaVuSans.ttf");
	im = gdImageCreate(800, 800);
	gdImageColorAllocate(im, 0xFF, 0xFF,
						 0xFF); /* allocate white for background color */
	black = gdImageColorAllocate(im, 0, 0, 0);
	cos_t = cos(DELTA);
	sin_t = sin(DELTA);
	x = 100;
	y = 0;

	for (i = 0; i < 16; i++) {
		if (gdImageStringFT(im, brect, black, path, 24, DELTA * i, 400 + x,
							400 + y, "ABCDEF")) {
			error = 1;
			goto done;
		}

		gdImagePolygon(im, (gdPointPtr)brect, 4, black);
		gdImageFilledEllipse(im, brect[0], brect[1], 8, 8, black);
		temp = (int)(cos_t * x + sin_t * y);
		y = (int)(cos_t * y - sin_t * x);
		x = temp;
	}

	ref = gdTestImageFromPng("gdimagestringft/gdimagestringft_bbox.png");
	if (!ref) {
		gdTestErrorMsg("failed to load reference image\n");
		error = 1;
		goto done;
	}
	gdImagePtr buf_diff = NULL;
	if (!gdTestAssert(gdImagePerceptualDiff(im, ref, PERCEPTUAL_THRESHOLD, NULL,
										  &buf_diff, &result))) {
		error = 1;
		goto done;
	}
	if (result.pixels_changed > MAX_PERCEPTUAL_PIXELS_CHANGED) {
		FILE *fp = fopen("gdimagestringft_bbox_diff.png", "wb");
		if (fp) {
			gdImagePng(buf_diff, fp);
			fclose(fp);
		}
		gdTestErrorMsg(
			"perceptual diff changed %u pixels (allowed=%u, threshold=%.2f)\n",
			result.pixels_changed, MAX_PERCEPTUAL_PIXELS_CHANGED,
			PERCEPTUAL_THRESHOLD);
		error = 1;
		FILE *f;
		f = fopen("/tmp/gd_test_diff.png", "wb");
		gdImagePng(buf_diff, f);
		fclose(f);

		f = fopen("/tmp/gd_test_ref.png", "wb");
		gdImagePng(ref, f);
		fclose(f);

		f = fopen("/tmp/gd_test_out.png", "wb");
		gdImagePng(im, f);
		fclose(f);
	}
	FILE *f;
	f = fopen("gdimagestringft_bbox.png", "wb");
	gdImagePng(im, f);
	fclose(f);
done:
	if (buf_diff) {
		gdImageDestroy(buf_diff);
	}
	if (ref) {
		gdImageDestroy(ref);
	}
	if (im) {
		gdImageDestroy(im);
	}
	gdFontCacheShutdown();
	if (path) {
		free(path);
	}
	return error;
}
