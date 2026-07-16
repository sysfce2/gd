#include "gd.h"
#include "gdtest.h"

#include <limits.h>
#include <string.h>

static gdImagePtr create_image(void)
{
	gdImagePtr im;
	int x, y;

	im = gdImageCreateTrueColor(16, 8);
	if (im == NULL) {
		return NULL;
	}

	for (y = 0; y < gdImageSY(im); y++) {
		for (x = 0; x < gdImageSX(im); x++) {
			gdImageSetPixel(im, x, y,
							gdTrueColor(x * 16, y * 32, 0x55));
		}
	}

	return im;
}

static void assert_dimensions(gdImagePtr im, int width, int height)
{
	gdTestAssert(im != NULL);
	if (im == NULL) {
		return;
	}
	gdTestAssert(gdImageSX(im) == width);
	gdTestAssert(gdImageSY(im) == height);
}

static int check_scaled_decode(void *jpeg, int size, unsigned int scale_num,
							   unsigned int scale_denom, int width,
							   int height)
{
	gdJpegReadOptions options;
	gdImagePtr decoded;

	gdJpegReadOptionsInit(&options);
	options.scale_num = scale_num;
	options.scale_denom = scale_denom;

	decoded = gdImageCreateFromJpegPtrWithOptions(size, jpeg, &options);
	assert_dimensions(decoded, width, height);
	if (decoded != NULL) {
		gdImageDestroy(decoded);
	}

	return gdNumFailures();
}

static void check_decode_fails(void *jpeg, int size,
							   const gdJpegReadOptions *options)
{
	gdImagePtr decoded;

	decoded = gdImageCreateFromJpegPtrWithOptions(size, jpeg, options);
	gdTestAssert(decoded == NULL);
	if (decoded != NULL) {
		gdImageDestroy(decoded);
	}
}

int main(void)
{
	static const struct {
		unsigned int scale_num;
		unsigned int scale_denom;
		int width;
		int height;
	} scales[] = {{1, 1, 16, 8}, {1, 2, 8, 4}, {1, 4, 4, 2},
				  {1, 8, 2, 1},  {4, 8, 8, 4}, {3, 8, 6, 3},
				  {2, 1, 32, 16}};
	static const int dct_methods[] = {GD_JPEG_DCT_DEFAULT, GD_JPEG_DCT_SLOW,
									  GD_JPEG_DCT_FAST, GD_JPEG_DCT_FLOAT};
	static const unsigned char not_jpeg[] = "not jpeg";
	gdImagePtr im;
	gdImagePtr decoded;
	gdIOCtx *ctx;
	gdJpegReadOptions options;
	void *jpeg;
	int size = 0;
	int i;

	im = create_image();
	gdTestAssert(im != NULL);
	if (im == NULL) {
		return gdNumFailures();
	}

	jpeg = gdImageJpegPtr(im, &size, 90);
	gdTestAssert(jpeg != NULL);
	gdTestAssert(size > 0);
	gdImageDestroy(im);
	if (jpeg == NULL || size <= 0) {
		return gdNumFailures();
	}

	gdJpegReadOptionsInit(&options);
	gdTestAssert(options.ignore_warning == 1);
	gdTestAssert(options.scale_num == 1);
	gdTestAssert(options.scale_denom == 1);
	gdTestAssert(options.dct_method == GD_JPEG_DCT_DEFAULT);

	for (i = 0; i < (int)(sizeof(scales) / sizeof(scales[0])); i++) {
		check_scaled_decode(jpeg, size, scales[i].scale_num,
							scales[i].scale_denom, scales[i].width,
							scales[i].height);
	}

	gdJpegReadOptionsInit(&options);
	options.scale_num = 1;
	options.scale_denom = 2;
	ctx = gdNewDynamicCtxEx(size, jpeg, 0);
	gdTestAssert(ctx != NULL);
	if (ctx != NULL) {
		decoded = gdImageCreateFromJpegCtxWithOptions(ctx, &options);
		assert_dimensions(decoded, 8, 4);
		if (decoded != NULL) {
			gdImageDestroy(decoded);
		}
		ctx->gd_free(ctx);
	}

	for (i = 0; i < (int)(sizeof(dct_methods) / sizeof(dct_methods[0])); i++) {
		gdJpegReadOptionsInit(&options);
		options.dct_method = dct_methods[i];
		decoded = gdImageCreateFromJpegPtrWithOptions(size, jpeg, &options);
		assert_dimensions(decoded, 16, 8);
		if (decoded != NULL) {
			gdImageDestroy(decoded);
		}
	}

	gdSetErrorMethod(gdSilence);

	gdJpegReadOptionsInit(&options);
	options.scale_num = 0;
	check_decode_fails(jpeg, size, &options);

	gdJpegReadOptionsInit(&options);
	options.scale_denom = 0;
	check_decode_fails(jpeg, size, &options);

	gdJpegReadOptionsInit(&options);
	options.dct_method = 99;
	check_decode_fails(jpeg, size, &options);

	gdJpegReadOptionsInit(&options);
	options.scale_num = 1;
	options.scale_denom = 99;
	check_decode_fails(jpeg, size, &options);

	gdJpegReadOptionsInit(&options);
	options.scale_num = UINT_MAX;
	options.scale_denom = 1;
	check_decode_fails(jpeg, size, &options);

	gdJpegReadOptionsInit(&options);
	options.scale_num = 1;
	options.scale_denom = 2;
	check_decode_fails((void *)not_jpeg, (int)sizeof(not_jpeg) - 1,
					   &options);

	gdClearErrorMethod();
	gdFree(jpeg);

	return gdNumFailures();
}
