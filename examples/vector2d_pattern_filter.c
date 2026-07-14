#include "vector2d_example.h"
#include "gdfonts.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SRC_SIZE 128
#define PANEL_W 150
#define PANEL_H 130
#define LABEL_H 18
#define GAP 14
#define LEFT 76
#define TOP 28

static int color(int r, int g, int b) { return gdTrueColor(r, g, b); }

static void label(gdImagePtr im, int x, int y, const char *text)
{
    gdImageString(im, gdFontGetSmall(), x, y, (unsigned char *)text, color(30, 30, 30));
}

static gdImagePtr create_source(int kind)
{
    gdImagePtr im = vector2d_create_image(SRC_SIZE, SRC_SIZE, gdTrueColorAlpha(255, 255, 255, 0));

    if (!im)
        return NULL;
    gdImageAlphaBlending(im, 0);
    for (int y = 0; y < SRC_SIZE; y++) {
        for (int x = 0; x < SRC_SIZE; x++) {
            int c;
            if (kind == 0) {
                int bit = ((x / 2) + (y / 2)) & 1;
                c = bit ? gdTrueColorAlpha(20, 20, 20, 0) : gdTrueColorAlpha(245, 245, 245, 0);
            } else if (kind == 1) {
                double dx = x - SRC_SIZE / 2.0;
                double dy = y - SRC_SIZE / 2.0;
                double r = sqrt(dx * dx + dy * dy);
                int bit = ((int)(r * r * 0.08)) & 1;
                c = bit ? gdTrueColorAlpha(35, 45, 150, 0) : gdTrueColorAlpha(245, 210, 70, 0);
            } else if (kind == 2) {
                int bit = ((x + y * 3) / 4) & 1;
                c = bit ? gdTrueColorAlpha(190, 35, 65, 0) : gdTrueColorAlpha(30, 145, 120, 0);
            } else if (kind == 3) {
                double dx = x - SRC_SIZE / 2.0;
                double dy = y - SRC_SIZE / 2.0;
                double r = sqrt(dx * dx + dy * dy);
                int alpha = r < 36.0 ? 0 : (r < 44.0 ? 64 : 127);
                c = gdTrueColorAlpha(255, 255, 255, alpha);
            } else {
                int n = (x * 37 + y * 73 + (x * y) * 5) & 255;
                int edge = (x > 28 && x < 96 && y > 38 && y < 88) ? 80 : 0;
                c = gdTrueColorAlpha((x * 2 + edge) & 255, (y * 2 + n / 5) & 255,
                                     (180 + n / 3) & 255, 0);
            }
            gdImageSetPixel(im, x, y, c);
        }
    }
    gdImageAlphaBlending(im, 1);
    return im;
}

static gdImagePtr render_case(gdImagePtr src, gdPatternFilter filter)
{
    gdImagePtr dst = vector2d_create_image(PANEL_W, PANEL_H, gdTrueColorAlpha(248, 248, 248, 0));
    gdContextPtr ctx;

    if (!dst)
        return NULL;
    ctx = gdContextCreateForImage(dst);
    if (!ctx) {
        gdImageDestroy(dst);
        return NULL;
    }
    gdContextSetPatternFilter(ctx, filter);
    gdContextTranslate(ctx, PANEL_W / 2.0, PANEL_H / 2.0);
    gdContextRotate(ctx, M_PI / 7.0);
    gdContextScale(ctx, 0.55, 0.55);
    gdContextTranslate(ctx, -SRC_SIZE / 2.0, -SRC_SIZE / 2.0);
    gdContextSetSourceImage(ctx, src, 0, 0);
    gdContextPaint(ctx);
    gdContextDestroy(ctx);
    return dst;
}

static void paste(gdImagePtr page, gdImagePtr panel, int x, int y)
{
    gdImageCopy(page, panel, x, y, 0, 0, gdImageSX(panel), gdImageSY(panel));
    gdImageRectangle(page, x, y, x + gdImageSX(panel) - 1, y + gdImageSY(panel) - 1,
                     color(160, 160, 160));
}

int main(void)
{
    const char *rows[] = {"checker", "zone", "stripes", "alpha", "photo-like"};
    const char *cols[] = {"source", "fast", "good", "best"};
    const int page_w = LEFT * 2 + 4 * PANEL_W + 3 * GAP;
    const int page_h = TOP * 2 + 5 * (PANEL_H + LABEL_H) + 4 * GAP;
    gdImagePtr page = vector2d_create_image(page_w, page_h, gdTrueColorAlpha(238, 240, 242, 0));

    if (!page)
        return 1;
    for (int col = 0; col < 4; col++)
        label(page, LEFT + col * (PANEL_W + GAP), 8, cols[col]);

    for (int row = 0; row < 5; row++) {
        gdImagePtr src = create_source(row);
        gdImagePtr panels[3];
        int y = TOP + row * (PANEL_H + LABEL_H + GAP);

        if (!src)
            return 1;
        label(page, 4, y + PANEL_H / 2, rows[row]);
        paste(page, src, LEFT, y);
        panels[0] = render_case(src, GD_PATTERN_FILTER_FAST);
        panels[1] = render_case(src, GD_PATTERN_FILTER_GOOD);
        panels[2] = render_case(src, GD_PATTERN_FILTER_BEST);
        for (int col = 0; col < 3; col++) {
            if (!panels[col])
                return 1;
            paste(page, panels[col], LEFT + (col + 1) * (PANEL_W + GAP), y);
            gdImageDestroy(panels[col]);
        }
        gdImageDestroy(src);
    }

    vector2d_save_png(page, "vector2d_pattern_filter.png");
    gdImageDestroy(page);
    return 0;
}
