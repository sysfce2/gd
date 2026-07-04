#include "gd_vector2d.h"
#include "gdtest.h"

#include <stdio.h>
#include <string.h>

#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 192
#define PERCEPTUAL_THRESHOLD .01

static void draw_line_path(gdContextPtr context, int relative)
{
	gdContextNewPath(context);
	if (relative) {
		gdContextRelMoveTo(context, 20, 25);
		gdContextRelLineTo(context, 50, -5);
		gdContextRelLineTo(context, 15, 45);
		gdContextRelLineTo(context, -55, 10);
	} else {
		gdContextMoveTo(context, 20, 25);
		gdContextLineTo(context, 70, 20);
		gdContextLineTo(context, 85, 65);
		gdContextLineTo(context, 30, 75);
	}
	gdContextClosePath(context);
	gdContextSetSourceRgba(context, .15, .55, .9, .75);
	gdContextFillPreserve(context);
	gdContextSetSourceRgb(context, .05, .15, .3);
	gdContextSetLineWidth(context, 3);
	gdContextStroke(context);
}

static void draw_quadratic_path(gdContextPtr context, int relative)
{
	gdContextNewPath(context);
	if (relative) {
		gdContextRelMoveTo(context, 105, 20);
		gdContextRelQuadTo(context, 30, -15, 50, 15);
		gdContextRelLineTo(context, -10, 35);
		gdContextRelQuadTo(context, -20, 15, -45, -10);
	} else {
		gdContextMoveTo(context, 105, 20);
		gdContextQuadTo(context, 135, 5, 155, 35);
		gdContextLineTo(context, 145, 70);
		gdContextQuadTo(context, 125, 85, 100, 60);
	}
	gdContextClosePath(context);
	gdContextSetSourceRgba(context, .85, .35, .15, .7);
	gdContextFillPreserve(context);
	gdContextSetSourceRgb(context, .35, .1, .05);
	gdContextSetLineWidth(context, 4);
	gdContextStroke(context);
}

static void draw_cubic_path(gdContextPtr context, int relative)
{
	gdContextNewPath(context);
	if (relative) {
		gdContextRelMoveTo(context, 185, 20);
		gdContextRelCurveTo(context, 30, -20, 55, 5, 40, 30);
		gdContextRelCurveTo(context, -15, 25, -45, 15, -45, -10);
	} else {
		gdContextMoveTo(context, 185, 20);
		gdContextCurveTo(context, 215, 0, 240, 25, 225, 50);
		gdContextCurveTo(context, 210, 75, 180, 65, 180, 40);
	}
	gdContextClosePath(context);
	gdContextSetSourceRgba(context, .35, .75, .25, .7);
	gdContextFillPreserve(context);
	gdContextSetSourceRgb(context, .1, .3, .05);
	gdContextSetLineWidth(context, 3);
	gdContextStroke(context);
}

static void draw_arcs_and_rectangle(gdContextPtr context)
{
	gdContextNewPath(context);
	gdContextArc(context, 55, 135, 28, .25, 5.75);
	gdContextClosePath(context);
	gdContextSetSourceRgba(context, .7, .2, .65, .65);
	gdContextFillPreserve(context);
	gdContextSetSourceRgb(context, .25, .05, .25);
	gdContextSetLineWidth(context, 3);
	gdContextStroke(context);

	gdContextNewPath(context);
	gdContextNegativeArc(context, 125, 135, 28, 5.75, .25);
	gdContextSetSourceRgb(context, .1, .55, .55);
	gdContextSetLineWidth(context, 6);
	gdContextStroke(context);

	gdContextNewPath(context);
	gdContextRectangle(context, 175, 108, 60, 55);
	gdContextSetSourceRgba(context, .95, .75, .1, .7);
	gdContextFillPreserve(context);
	gdContextSetSourceRgb(context, .4, .3, .02);
	gdContextSetLineWidth(context, 5);
	gdContextStroke(context);
}

static gdImagePtr render_scene(int relative)
{
	gdImagePtr image = gdImageCreateTrueColor(IMAGE_WIDTH, IMAGE_HEIGHT);
	gdContextPtr context;

	if (!image)
		return NULL;
	context = gdContextCreateForImage(image);
	if (!context) {
		gdImageDestroy(image);
		return NULL;
	}

	gdContextSetSourceRgb(context, 1, 1, 1);
	gdContextPaint(context);
	draw_line_path(context, relative);
	draw_quadratic_path(context, relative);
	draw_cubic_path(context, relative);
	draw_arcs_and_rectangle(context);
	gdContextFlushImage(context);
	gdContextDestroy(context);
	return image;
}

static int write_reference(const char *filename)
{
	gdImagePtr image = render_scene(1);
	FILE *file;

	if (!image)
		return 1;
	file = fopen(filename, "wb");
	if (!file) {
		gdImageDestroy(image);
		return 1;
	}
	gdImagePng(image, file);
	fclose(file);
	gdImageDestroy(image);
	return 0;
}

int main(int argc, char **argv)
{
	gdImagePtr absolute;
	gdImagePtr relative;
	CuTestImageResult result = {0, 0};

	if (argc == 3 && strcmp(argv[1], "--write-reference") == 0)
		return write_reference(argv[2]);

	absolute = render_scene(0);
	relative = render_scene(1);
	gdTestAssert(absolute != NULL && relative != NULL);
	if (absolute && relative) {
		gdTestImagePerceptualDiff(absolute, relative, NULL, &result,
			PERCEPTUAL_THRESHOLD);
		gdTestAssertMsg(result.pixels_changed == 0,
			"absolute and relative paths differ at %u pixels",
			result.pixels_changed);
		gdTestAssert(gdAssertImageEqualsToFilePerceptual(
			"vector2d/path_ops_reference.png", relative,
			PERCEPTUAL_THRESHOLD, 0));
	}
	if (absolute)
		gdImageDestroy(absolute);
	if (relative)
		gdImageDestroy(relative);
	return gdNumFailures();
}
