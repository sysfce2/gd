#include "gd.h"
#include "gd_vector2d.h"
#include "gdtest.h"

static gdImagePtr image_of(int width, int height, int color)
{
	gdImagePtr image = gdImageCreateTrueColor(width, height);
	if (image != NULL) {
		gdImageAlphaBlending(image, 0);
		gdImageSaveAlpha(image, 1);
		gdImageFilledRectangle(image, 0, 0, width - 1, height - 1, color);
	}
	return image;
}

static void assert_pixel(const char *label, gdImagePtr image, int x, int y, int expected)
{
	int actual = gdImageGetTrueColorPixel(image, x, y);
	gdTestAssertMsg(actual == expected, "%s: got %08x, expected %08x", label, actual,
					expected);
}

static void test_basic_operators_and_opacity(void)
{
	gdImagePtr dest = image_of(3, 3, gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	gdImagePtr source = image_of(2, 2, gdTrueColorAlpha(127, 0, 0, gdAlphaTransparent));
	gdImagePtr source_op = image_of(1, 1, gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	gdImagePtr source_dest = image_of(1, 1, gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	gdImagePtr opacity_dest = image_of(1, 1, gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	gdImagePtr opacity_source = image_of(1, 1, gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));

	if (!gdTestAssert(dest != NULL && source != NULL && source_op != NULL && source_dest != NULL &&
				 opacity_dest != NULL && opacity_source != NULL)) {
		gdImageDestroy(dest); gdImageDestroy(source); gdImageDestroy(source_op);
		gdImageDestroy(source_dest); gdImageDestroy(opacity_dest); gdImageDestroy(opacity_source);
		return;
	}
	gdImageSetPixel(source, 0, 0, gdTrueColorAlpha(255, 0, 0, 64));
	gdImageSetPixel(source, 1, 0, gdTrueColorAlpha(0, 255, 0, gdAlphaTransparent));
	gdTestAssert(gdImageComposite(dest, source, 1, 1, GD_OP_OVER, 1.0, NULL, NULL));
	gdTestAssert(gdImageComposite(source_dest, source_op, 0, 0, GD_OP_SOURCE, 1.0, NULL, NULL));
	assert_pixel("source", source_dest, 0, 0, gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	gdTestAssert(gdImageComposite(opacity_dest, opacity_source, 0, 0, GD_OP_OVER, 0.5, NULL, NULL));
	assert_pixel("opacity", opacity_dest, 0, 0, gdTrueColorAlpha(128, 0, 128, gdAlphaOpaque));
	gdTestAssert(gdImageComposite(dest, source, 0, 0, GD_OP_DEST, 1.0, NULL, NULL));
	gdImageDestroy(dest); gdImageDestroy(source); gdImageDestroy(source_op);
	gdImageDestroy(source_dest); gdImageDestroy(opacity_dest); gdImageDestroy(opacity_source);
}

static void test_regions_clips_offsets_and_unbounded(void)
{
	gdImagePtr sheet = image_of(2, 1, gdTrueColorAlpha(0, 255, 0, gdAlphaOpaque));
	gdImagePtr cropped = image_of(1, 1, gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent));
	gdImagePtr clipped = image_of(3, 1, gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	gdImagePtr mask = image_of(1, 1, gdTrueColorAlpha(0, 0, 0, gdAlphaOpaque));
	gdImagePtr clip_source = image_of(3, 1, gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	gdImagePtr unbounded = image_of(3, 1, gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	gdRect full_clip = {0, 0, 3, 1};
	gdRect region = {1, 0, 1, 1}, clip = {1, 0, 1, 1};
	if (!gdTestAssert(sheet != NULL && cropped != NULL && clipped != NULL && mask != NULL && clip_source != NULL && unbounded != NULL)) {
		gdImageDestroy(sheet); gdImageDestroy(cropped); gdImageDestroy(clipped); gdImageDestroy(mask); gdImageDestroy(clip_source); gdImageDestroy(unbounded); return;
	}
	gdImageSetPixel(sheet, 1, 0, gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	gdTestAssert(gdImageComposite(cropped, sheet, 0, 0, GD_OP_SOURCE, 1.0, &region, NULL));
	assert_pixel("source region", cropped, 0, 0, gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	gdTestAssert(gdImageComposite(clipped, clip_source, 0, 0, GD_OP_SOURCE, 1.0, NULL, &clip));
	assert_pixel("clip left", clipped, 0, 0, gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	assert_pixel("clip middle", clipped, 1, 0, gdTrueColorAlpha(255, 0, 0, gdAlphaOpaque));
	assert_pixel("clip right", clipped, 2, 0, gdTrueColorAlpha(0, 0, 255, gdAlphaOpaque));
	gdTestAssert(gdImageComposite(unbounded, mask, 1, 0, GD_OP_DEST_IN, 1.0, NULL, &full_clip));
	gdTestAssertMsg(gdTrueColorGetAlpha(gdImageGetTrueColorPixel(unbounded, 0, 0)) == gdAlphaTransparent,
					"unbounded left: expected destination to be cleared");
	gdTestAssertMsg(gdTrueColorGetAlpha(gdImageGetTrueColorPixel(unbounded, 1, 0)) == gdAlphaOpaque,
					"unbounded middle: expected destination to be preserved");
	gdTestAssertMsg(gdTrueColorGetAlpha(gdImageGetTrueColorPixel(unbounded, 2, 0)) == gdAlphaTransparent,
					"unbounded right: expected destination to be cleared");
	gdImageDestroy(sheet); gdImageDestroy(cropped); gdImageDestroy(clipped); gdImageDestroy(mask); gdImageDestroy(clip_source); gdImageDestroy(unbounded);
}

int main(void)
{
	test_basic_operators_and_opacity();
	test_regions_clips_offsets_and_unbounded();
	return gdNumFailures();
}
