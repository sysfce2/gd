#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gd.h"
#include "gdfontl.h"
#include "gdfonts.h"

#define DEFAULT_SOURCE_DIR "examples/scale_entropy_source_images"
#define DEFAULT_OUTPUT "scale_entropy_visual_check.png"

#define MARGIN 24
#define GAP 18
#define SOURCE_W 220
#define PANEL_W 170
#define PANEL_H 190
#define ROW_H 235
#define TITLE_H 74

typedef struct {
	const char *file;
	const char *text;
	int target_w;
	int target_h;
} VisualCase;

static int color(int r, int g, int b)
{
	return gdTrueColor(r, g, b);
}

static void draw_text(gdImagePtr im, int x, int y, const char *text, int large)
{
	gdImageString(im, large ? gdFontGetLarge() : gdFontGetSmall(), x, y,
				  (unsigned char *)text, color(28, 28, 28));
}

static void draw_panel(gdImagePtr page, int x, int y, int w, int h, const char *label)
{
	gdImageFilledRectangle(page, x, y, x + w - 1, y + h - 1, color(255, 255, 255));
	gdImageRectangle(page, x, y, x + w - 1, y + h - 1, color(155, 155, 155));
	draw_text(page, x, y - 18, label, 0);
}

static gdImagePtr load_jpeg(const char *path)
{
	FILE *fp;
	gdImagePtr im;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open %s\n", path);
		return NULL;
	}

	im = gdImageCreateFromJpeg(fp);
	fclose(fp);
	if (im == NULL) {
		fprintf(stderr, "Cannot decode %s\n", path);
	}

	return im;
}

static gdImagePtr scale_to_fit(gdImagePtr src, int max_w, int max_h)
{
	const int src_w = gdImageSX(src);
	const int src_h = gdImageSY(src);
	int w, h;

	if ((long long)src_w * max_h > (long long)max_w * src_h) {
		w = max_w;
		h = (int)(((long long)src_h * max_w + src_w / 2) / src_w);
	} else {
		h = max_h;
		w = (int)(((long long)src_w * max_h + src_h / 2) / src_h);
	}

	if (w < 1) {
		w = 1;
	}
	if (h < 1) {
		h = 1;
	}

	gdImageSetInterpolationMethod(src, GD_BILINEAR_FIXED);
	return gdImageScale(src, (unsigned int)w, (unsigned int)h);
}

static int paste_fit(gdImagePtr page, gdImagePtr src, int x, int y, int w, int h)
{
	gdImagePtr preview;
	int dst_x, dst_y;

	preview = scale_to_fit(src, w - 12, h - 12);
	if (preview == NULL) {
		return 0;
	}

	dst_x = x + (w - gdImageSX(preview)) / 2;
	dst_y = y + (h - gdImageSY(preview)) / 2;
	gdImageCopy(page, preview, dst_x, dst_y, 0, 0, gdImageSX(preview),
				gdImageSY(preview));
	gdImageDestroy(preview);

	return 1;
}

static gdImagePtr scale_cover_size(gdImagePtr src, gdScaleStrategy strategy, int w, int h)
{
	gdScaleOptions options;

	options.fit = GD_SCALE_FIT_COVER;
	options.gravity = GD_SCALE_GRAVITY_CENTER;
	options.strategy = strategy;
	options.background_color = gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent);
	options.interpolation = GD_SCALE_INTERPOLATION_AUTO;

	return gdImageScaleWithOptions(src, (unsigned int)w, (unsigned int)h, &options);
}

static int render_case(gdImagePtr page, const VisualCase *item, const char *source_dir,
					   int row)
{
	char path[1024];
	gdImagePtr source, center, entropy, attention;
	const int y = TITLE_H + row * ROW_H + 44;
	const int source_x = MARGIN;
	const int center_x = source_x + SOURCE_W + GAP;
	const int entropy_x = center_x + PANEL_W + GAP;
	const int attention_x = entropy_x + PANEL_W + GAP;

	if (snprintf(path, sizeof(path), "%s/%s", source_dir, item->file) >=
		(int)sizeof(path)) {
		fprintf(stderr, "Path too long for %s\n", item->file);
		return 0;
	}

	source = load_jpeg(path);
	if (source == NULL) {
		return 0;
	}

	center = scale_cover_size(source, GD_SCALE_STRATEGY_NONE, item->target_w,
							  item->target_h);
	entropy = scale_cover_size(source, GD_SCALE_STRATEGY_ENTROPY, item->target_w,
							   item->target_h);
	attention = scale_cover_size(source, GD_SCALE_STRATEGY_ATTENTION,
								 item->target_w, item->target_h);
	if (center == NULL || entropy == NULL) {
		fprintf(stderr, "Scale failed for %s\n", item->file);
		gdImageDestroy(source);
		if (center != NULL) {
			gdImageDestroy(center);
		}
		if (entropy != NULL) {
			gdImageDestroy(entropy);
		}
		if (attention != NULL) {
			gdImageDestroy(attention);
		}
		return 0;
	}

	draw_text(page, MARGIN, y - 38, item->file, 1);
	draw_text(page, MARGIN + 240, y - 35, item->text, 0);

	draw_panel(page, source_x, y, SOURCE_W, PANEL_H, "Source");
	draw_panel(page, center_x, y, PANEL_W, PANEL_H, "Cover + Center");
	draw_panel(page, entropy_x, y, PANEL_W, PANEL_H, "Cover + Entropy");
	draw_panel(page, attention_x, y, PANEL_W, PANEL_H, "Cover + Attention");

	if (!paste_fit(page, source, source_x, y, SOURCE_W, PANEL_H) ||
		!paste_fit(page, center, center_x, y, PANEL_W, PANEL_H) ||
		!paste_fit(page, entropy, entropy_x, y, PANEL_W, PANEL_H)) {
		fprintf(stderr, "Preview scale failed for %s\n", item->file);
		gdImageDestroy(source);
		gdImageDestroy(center);
		gdImageDestroy(entropy);
		if (attention != NULL) {
			gdImageDestroy(attention);
		}
		return 0;
	}

	if (attention != NULL) {
		if (!paste_fit(page, attention, attention_x, y, PANEL_W, PANEL_H)) {
			fprintf(stderr, "Attention preview scale failed for %s\n", item->file);
			gdImageDestroy(source);
			gdImageDestroy(center);
			gdImageDestroy(entropy);
			gdImageDestroy(attention);
			return 0;
		}
	} else {
		draw_text(page, attention_x + 14, y + 76, "Unavailable", 0);
	}

	gdImageDestroy(source);
	gdImageDestroy(center);
	gdImageDestroy(entropy);
	if (attention != NULL) {
		gdImageDestroy(attention);
	}

	return 1;
}

static int save_png(gdImagePtr page, const char *path)
{
	FILE *fp;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Cannot write %s\n", path);
		return 0;
	}

	gdImagePng(page, fp);
	fclose(fp);
	return 1;
}

int main(int argc, char **argv)
{
	const VisualCase cases[] = {
		{"portrait_sitting.jpg", "Portrait: attention should keep the subject.",
		 120, 180},
		{"portrait_right_side.jpg", "Off-center portrait: compare subject crop.",
		 120, 180},
		{"old_kid_portrait.jpg", "Centered portrait: content crop should stay calm.",
		 120, 180},
		{"family_us_low_contrast.jpg", "People in busy scene: attention vs texture.",
		 260, 180},
		{"new_machine.jpg", "Texture scene: entropy is the useful mode.", 260, 180},
		{"landscape_1.jpg", "Landscape: detail crop compared with center.", 260, 180},
	};
	const char *source_dir = argc > 1 ? argv[1] : DEFAULT_SOURCE_DIR;
	const char *output = argc > 2 ? argv[2] : DEFAULT_OUTPUT;
	const int count = (int)(sizeof(cases) / sizeof(cases[0]));
	const int page_w = MARGIN * 2 + SOURCE_W + GAP + PANEL_W + GAP + PANEL_W +
					   GAP + PANEL_W;
	const int page_h = TITLE_H + count * ROW_H + MARGIN;
	gdImagePtr page;
	int i;

	page = gdImageCreateTrueColor(page_w, page_h);
	if (page == NULL) {
		fprintf(stderr, "Cannot create output page\n");
		return 1;
	}

	gdImageFilledRectangle(page, 0, 0, page_w - 1, page_h - 1,
						   color(244, 244, 244));
	draw_text(page, MARGIN, 22, "Scale content-aware visual check", 1);
	draw_text(page, MARGIN, 48,
			  "Modes: Cover + Center, Cover + Entropy, Cover + Attention", 0);

	for (i = 0; i < count; i++) {
		if (!render_case(page, &cases[i], source_dir, i)) {
			gdImageDestroy(page);
			return 2;
		}
	}

	if (!save_png(page, output)) {
		gdImageDestroy(page);
		return 3;
	}

	printf("Wrote %s\n", output);
	gdImageDestroy(page);
	return 0;
}
