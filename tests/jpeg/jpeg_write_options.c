#include "gd.h"
#include "gdtest.h"

#include <jpeglib.h>
#include <stdio.h>
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
							gdTrueColor(x * 16, y * 32, (x + y) * 8));
		}
	}

	return im;
}

static int check_no_subsampling(void *jpeg, int size)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *fp;
	int i;
	int result = 1;

	fp = tmpfile();
	gdTestAssert(fp != NULL);
	if (fp == NULL) {
		return 0;
	}
	gdTestAssert(fwrite(jpeg, 1, size, fp) == (size_t)size);
	rewind(fp);

	memset(&cinfo, 0, sizeof(cinfo));
	memset(&jerr, 0, sizeof(jerr));
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);

	gdTestAssert(jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK);
	for (i = 0; i < cinfo.num_components; i++) {
		if (cinfo.comp_info[i].h_samp_factor != 1 ||
			cinfo.comp_info[i].v_samp_factor != 1) {
			result = 0;
		}
	}
	gdTestAssert(result);

	jpeg_destroy_decompress(&cinfo);
	fclose(fp);
	return result;
}

int main(void)
{
	gdImagePtr im;
	gdImagePtr decoded;
	gdJpegInfo info;
	gdJpegWriteOptions options;
	void *legacy;
	void *advanced;
	int legacy_size = 0;
	int advanced_size = 0;

	im = create_image();
	gdTestAssert(im != NULL);
	if (im == NULL) {
		return gdNumFailures();
	}

	gdJpegWriteOptionsInit(&options);
	gdTestAssert(options.quality == -1);
	gdTestAssert(options.progressive == 0);
	gdTestAssert(options.force_no_subsampling == 0);
	gdTestAssert(options.metadata == NULL);

	legacy = gdImageJpegPtr(im, &legacy_size, -1);
	advanced = gdImageJpegPtrWithOptions(im, &advanced_size, NULL);
	gdTestAssert(legacy != NULL);
	gdTestAssert(advanced != NULL);
	gdTestAssert(legacy_size == advanced_size);
	if (legacy != NULL && advanced != NULL && legacy_size == advanced_size) {
		gdTestAssert(memcmp(legacy, advanced, legacy_size) == 0);
	}
	if (legacy != NULL) {
		gdFree(legacy);
	}
	if (advanced != NULL) {
		gdFree(advanced);
	}

	gdJpegWriteOptionsInit(&options);
	options.progressive = 1;
	advanced = gdImageJpegPtrWithOptions(im, &advanced_size, &options);
	gdTestAssert(advanced != NULL);
	gdTestAssert(advanced_size > 0);
	if (advanced != NULL) {
		gdTestAssert(gdJpegGetInfoPtr(advanced_size, advanced, &info) == 0);
		gdTestAssert(info.width == 16);
		gdTestAssert(info.height == 8);
		gdTestAssert(info.progressive == 1);
		gdFree(advanced);
	}

	gdJpegWriteOptionsInit(&options);
	options.quality = 90;
	advanced_size = 0;
	advanced = gdImageJpegPtrWithOptions(im, &advanced_size, &options);
	gdTestAssert(advanced != NULL);
	gdTestAssert(advanced_size > 0);
	if (advanced != NULL) {
		decoded = gdImageCreateFromJpegPtr(advanced_size, advanced);
		gdTestAssert(decoded != NULL);
		if (decoded != NULL) {
			gdTestAssert(gdImageSX(decoded) == 16);
			gdTestAssert(gdImageSY(decoded) == 8);
			gdImageDestroy(decoded);
		}
		gdFree(advanced);
	}

	gdJpegWriteOptionsInit(&options);
	options.quality = 50;
	options.force_no_subsampling = 1;
	advanced_size = 0;
	advanced = gdImageJpegPtrWithOptions(im, &advanced_size, &options);
	gdTestAssert(advanced != NULL);
	gdTestAssert(advanced_size > 0);
	if (advanced != NULL) {
		check_no_subsampling(advanced, advanced_size);
		gdFree(advanced);
	}

	gdImageDestroy(im);
	return gdNumFailures();
}
