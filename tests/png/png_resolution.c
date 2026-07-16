#include "gd.h"
#include "gdtest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DPI2DPM(dpi) ((unsigned int)((dpi) / 0.0254 + 0.5))

static unsigned int get_uint32(const unsigned char *data)
{
	return ((unsigned int)data[0] << 24) | ((unsigned int)data[1] << 16) |
		   ((unsigned int)data[2] << 8) | (unsigned int)data[3];
}

static unsigned char *find_chunk(unsigned char *png, int size, const char *type)
{
	size_t pos = 8;

	while (pos + 12 <= (size_t)size) {
		unsigned int chunk_size = get_uint32(png + pos);

		if ((size_t)chunk_size > (size_t)size - pos - 12) {
			return NULL;
		}
		if (memcmp(png + pos + 4, type, 4) == 0) {
			return png + pos;
		}
		pos += (size_t)chunk_size + 12;
	}

	return NULL;
}

int main() {
	gdImagePtr im;
	gdPngInfo info;
	unsigned char *phys;
	void *data;
	int size, red;

	im = gdImageCreate(100, 100);
	gdImageSetResolution(im, 72, 300);
	red = gdImageColorAllocate(im, 0xFF, 0x00, 0x00);
	gdImageFilledRectangle(im, 0, 0, 99, 99, red);
	data = gdImagePngPtr(im, &size);
	gdImageDestroy(im);

	gdPngInfoInit(&info);
	if (!gdTestAssert(gdPngGetInfoPtr(size, data, &info) == 0) ||
		!gdTestAssert(info.x_pixels_per_unit == (int)TEST_DPI2DPM(72)) ||
		!gdTestAssert(info.y_pixels_per_unit == (int)TEST_DPI2DPM(300)) ||
		!gdTestAssert(info.physical_unit == 1) ||
		!gdTestAssert(info.resolution_x == 72) ||
		!gdTestAssert(info.resolution_y == 300)) {
		gdTestErrorMsg(
			"failed PNG info resolution raw (%d,%d,%d) dpi (%d,%d)\n",
			info.x_pixels_per_unit, info.y_pixels_per_unit, info.physical_unit,
			info.resolution_x, info.resolution_y);
		gdFree(data);
		return 1;
	}

	phys = find_chunk((unsigned char *)data, size, "pHYs");
	if (!gdTestAssert(phys != NULL)) {
		gdFree(data);
		return 1;
	}
	phys[16] = 0;
	gdPngInfoInit(&info);
	if (!gdTestAssert(gdPngGetInfoPtr(size, data, &info) == 0) ||
		!gdTestAssert(info.x_pixels_per_unit == (int)TEST_DPI2DPM(72)) ||
		!gdTestAssert(info.y_pixels_per_unit == (int)TEST_DPI2DPM(300)) ||
		!gdTestAssert(info.physical_unit == 0) ||
		!gdTestAssert(info.resolution_x == -1) ||
		!gdTestAssert(info.resolution_y == -1)) {
		gdTestErrorMsg(
			"failed unitless PNG info raw (%d,%d,%d) dpi (%d,%d)\n",
			info.x_pixels_per_unit, info.y_pixels_per_unit, info.physical_unit,
			info.resolution_x, info.resolution_y);
		gdFree(data);
		return 1;
	}
	phys[16] = 1;

	im = gdImageCreateFromPngPtr(size, data);
	gdFree(data);
	if (!gdTestAssert(im != NULL)) {
		return 1;
	}

	if (!gdTestAssert(gdImageResolutionX(im) == 72) ||
		!gdTestAssert(gdImageResolutionY(im) == 300)) {
		gdTestErrorMsg(
			"failed image resolution X (%d != 72) or Y (%d != 300)\n",
			gdImageResolutionX(im), gdImageResolutionY(im));
		gdImageDestroy(im);
		return 1;
	}

	gdImageDestroy(im);
	return 0;
}
