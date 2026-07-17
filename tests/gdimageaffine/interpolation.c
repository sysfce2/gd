#include "gd.h"
#include "gd_path_matrix.h"
#include "gdtest.h"
#include <math.h>

static gdImagePtr edge_fixture(void)
{
	const int w = 240, h = 180;
	gdImagePtr im = gdImageCreateTrueColor(w, h);
	if (im == NULL) return NULL;
	gdImageAlphaBlending(im, 0);
	gdImageSaveAlpha(im, 1);
	gdImageFilledRectangle(im, 0, 0, w - 1, h - 1, gdTrueColorAlpha(255, 0, 255, 127));
	gdImageFilledRectangle(im, 0, 0, w - 1, 34, gdTrueColorAlpha(59, 130, 208, 0));
	gdImageFilledRectangle(im, 0, 145, w - 1, h - 1, gdTrueColorAlpha(196, 59, 120, 0));
	gdImageFilledRectangle(im, 0, 35, 34, 144, gdTrueColorAlpha(71, 173, 104, 0));
	gdImageFilledRectangle(im, 205, 35, w - 1, 144, gdTrueColorAlpha(225, 139, 50, 0));
	gdImageFilledRectangle(im, 35, 35, 70, 144, gdTrueColorAlpha(53, 183, 212, 80));
	gdImageFilledRectangle(im, 169, 35, 204, 144, gdTrueColorAlpha(63, 101, 216, 112));
	gdImageFilledRectangle(im, 71, 35, 168, 70, gdTrueColorAlpha(37, 196, 106, 56));
	gdImageFilledRectangle(im, 71, 109, 168, 144, gdTrueColorAlpha(48, 208, 176, 96));
	gdImageFilledEllipse(im, 120, 89, 72, 58, gdTrueColorAlpha(255, 0, 255, 127));
	gdImageFilledEllipse(im, 120, 89, 48, 36, gdTrueColorAlpha(240, 208, 64, 0));
	return im;
}

static void matrix_to_affine(const gdPathMatrixPtr matrix, double affine[6])
{
	affine[0] = matrix->m00;
	affine[1] = matrix->m10;
	affine[2] = matrix->m01;
	affine[3] = matrix->m11;
	affine[4] = matrix->m02;
	affine[5] = matrix->m12;
}

static void test_transparent_edges(void)
{
	gdImagePtr src = edge_fixture(), affine = NULL, rotated = NULL;
	double matrix[6];
	gdPathMatrix path_matrix;
	if (!gdTestAssert(src != NULL)) return;
	gdImageSetInterpolationMethod(src, GD_CATMULLROM);
	gdPathMatrixInitIdentity(&path_matrix);
	gdPathMatrixRotateTranslate(&path_matrix, M_PI / 11.0, 120.0, 90.0);
	gdPathMatrixShear(&path_matrix, 0.10, -0.06);
	gdPathMatrixTranslate(&path_matrix, 10.0, -7.0);
	matrix_to_affine(&path_matrix, matrix);
	gdTestAssert(gdTransformAffineGetImage(&affine, src, NULL, matrix));
	rotated = gdImageRotateInterpolated(src, 27.0f, gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent));
	gdTestAssert(affine != NULL && rotated != NULL);
	if (affine != NULL) {
		gdAssertImageEqualsToFilePerceptual("gdimageaffine/interpolation_edges_affine.png",
											 affine, 0.03, 100);
	}
	if (rotated != NULL) {
		gdAssertImageEqualsToFilePerceptual("gdimageaffine/interpolation_edges_rotate.png",
											 rotated, 0.03, 100);
	}
	gdImageDestroy(rotated);
	gdImageDestroy(affine);
	gdImageDestroy(src);
}

static gdImagePtr negative_fixture(int padding)
{
	gdImagePtr im = gdImageCreateTrueColor(6 + 2 * padding, 5 + 2 * padding);
	static const int colors[] = {0x2070d0, 0xe03040, 0x30b060, 0xf0b020, 0x7040c0, 0x20c0c0};
	int x, y;
	if (im == NULL) return NULL;
	gdImageAlphaBlending(im, 0);
	gdImageSaveAlpha(im, 1);
	gdImageFilledRectangle(im, 0, 0, gdImageSX(im) - 1, gdImageSY(im) - 1,
						gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent));
	for (y = 0; y < 5; y++) for (x = 0; x < 6; x++)
		gdImageSetPixel(im, padding + x, padding + y, colors[(x + 2 * y) % 6]);
	gdImageSetInterpolationMethod(im, GD_CATMULLROM);
	return im;
}

static void assert_center_equal(gdImagePtr actual, gdImagePtr padded, const char *label)
{
	int x, y, dx = (gdImageSX(padded) - gdImageSX(actual)) / 2;
	int dy = (gdImageSY(padded) - gdImageSY(actual)) / 2;
	gdTestAssertMsg(gdImageSX(padded) >= gdImageSX(actual) && gdImageSY(padded) >= gdImageSY(actual),
					"%s: transformed dimensions differ: %dx%d vs %dx%d", label,
					gdImageSX(actual), gdImageSY(actual), gdImageSX(padded), gdImageSY(padded));
	if (gdImageSX(padded) < gdImageSX(actual) || gdImageSY(padded) < gdImageSY(actual)) return;
	for (y = 0; y < gdImageSY(actual); y++) for (x = 0; x < gdImageSX(actual); x++)
		gdTestAssertMsg(gdImageGetTrueColorPixel(actual, x, y) ==
						gdImageGetTrueColorPixel(padded, x + dx, y + dy),
						"%s: pixel differs at %d,%d", label, x, y);
}

static void assert_cropped_equal(gdImagePtr actual, gdImagePtr padded, const char *label)
{
	int x, y;
	int dx = (gdImageSX(padded) - gdImageSX(actual)) / 2;
	int dy = (gdImageSY(padded) - gdImageSY(actual)) / 2;
	gdTestAssertMsg(gdImageSX(padded) >= gdImageSX(actual) &&
					gdImageSY(padded) >= gdImageSY(actual),
					"%s: rotated dimensions differ: %dx%d vs %dx%d", label,
					gdImageSX(actual), gdImageSY(actual), gdImageSX(padded), gdImageSY(padded));
	if (gdImageSX(padded) < gdImageSX(actual) || gdImageSY(padded) < gdImageSY(actual)) return;
	for (y = 0; y < gdImageSY(actual); y++) for (x = 0; x < gdImageSX(actual); x++)
		gdTestAssertMsg(gdImageGetTrueColorPixel(actual, x, y) ==
						gdImageGetTrueColorPixel(padded, x + dx, y + dy),
						"%s: pixel differs at %d,%d", label, x, y);
}

static void test_negative_fractional_coordinates(void)
{
	gdImagePtr src = negative_fixture(0), padded = negative_fixture(4);
	gdImagePtr actual = NULL, expected = NULL, rotated = NULL, padded_rotated = NULL;
	double matrix[6], padded_matrix[6];
	gdPathMatrix path_matrix;
	gdPathMatrix padded_path_matrix;
	gdRect area = {0, 0, 6, 5};
	gdPathMatrixInitIdentity(&path_matrix);
	gdPathMatrixRotateTranslate(&path_matrix, M_PI / 7.0, 3.0, 2.5);
	matrix_to_affine(&path_matrix, matrix);
	gdPathMatrixInitIdentity(&padded_path_matrix);
	gdPathMatrixRotateTranslate(&padded_path_matrix, M_PI / 7.0, 7.0, 6.5);
	matrix_to_affine(&padded_path_matrix, padded_matrix);
	if (!gdTestAssert(src != NULL && padded != NULL)) goto done;
	gdTestAssert(gdTransformAffineGetImage(&actual, src, &area, matrix));
	gdTestAssert(gdTransformAffineGetImage(&expected, padded, NULL, padded_matrix));
	if (actual != NULL && expected != NULL) assert_center_equal(actual, expected, "catmull-rom negative coordinates");
	gdImageSetInterpolationMethod(src, GD_WEIGHTED4);
	gdImageSetInterpolationMethod(padded, GD_WEIGHTED4);
	gdTestAssert(gdTransformAffineGetImage(&actual, src, &area, matrix));
	gdTestAssert(gdTransformAffineGetImage(&expected, padded, NULL, padded_matrix));
	if (actual != NULL && expected != NULL) assert_center_equal(actual, expected, "weighted-4 negative coordinates");
	gdImageSetInterpolationMethod(src, GD_CATMULLROM);
	gdImageSetInterpolationMethod(padded, GD_CATMULLROM);
	rotated = gdImageRotateInterpolated(src, 180.0 / 7.0, gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent));
	padded_rotated = gdImageRotateInterpolated(padded, 180.0 / 7.0, gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent));
	if (rotated != NULL && padded_rotated != NULL) assert_cropped_equal(rotated, padded_rotated, "rotate negative coordinates");
done:
	gdImageDestroy(rotated); gdImageDestroy(padded_rotated); gdImageDestroy(actual); gdImageDestroy(expected);
	gdImageDestroy(src); gdImageDestroy(padded);
}

int main(void)
{
	test_transparent_edges();
	test_negative_fractional_coordinates();
	return gdNumFailures();
}
