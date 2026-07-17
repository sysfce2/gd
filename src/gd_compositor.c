#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gd_compositor.h"
#include "gd_vector2d_private.h"

typedef enum {
    FACTOR_ZERO,
    FACTOR_ONE,
    FACTOR_SRC_ALPHA,
    FACTOR_DST_ALPHA,
    FACTOR_ONE_MINUS_SRC_ALPHA,
    FACTOR_ONE_MINUS_DST_ALPHA,
    FACTOR_SATURATE
} FactorKind;

typedef struct {
    FactorKind src, dst;
} Factors;

static const Factors porter_duff[] = {{FACTOR_ZERO, FACTOR_ZERO},
                                      {FACTOR_ONE, FACTOR_ZERO},
                                      {FACTOR_ONE, FACTOR_ONE_MINUS_SRC_ALPHA},
                                      {FACTOR_DST_ALPHA, FACTOR_ZERO},
                                      {FACTOR_ONE_MINUS_DST_ALPHA, FACTOR_ZERO},
                                      {FACTOR_DST_ALPHA, FACTOR_ONE_MINUS_SRC_ALPHA},
                                      {FACTOR_ZERO, FACTOR_ONE},
                                      {FACTOR_ONE_MINUS_DST_ALPHA, FACTOR_ONE},
                                      {FACTOR_ZERO, FACTOR_SRC_ALPHA},
                                      {FACTOR_ZERO, FACTOR_ONE_MINUS_SRC_ALPHA},
                                      {FACTOR_ONE_MINUS_DST_ALPHA, FACTOR_SRC_ALPHA},
                                      {FACTOR_ONE_MINUS_DST_ALPHA, FACTOR_ONE_MINUS_SRC_ALPHA},
                                      {FACTOR_ONE, FACTOR_ONE},
                                      {FACTOR_SATURATE, FACTOR_ONE}};

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static gdPremulPixelF clamp_pixel(gdPremulPixelF p)
{
    p.a = clamp01(p.a);
    p.r = fminf(clamp01(p.r), p.a);
    p.g = fminf(clamp01(p.g), p.a);
    p.b = fminf(clamp01(p.b), p.a);
    return p;
}

static gdPremulPixelF scale_pixel(gdPremulPixelF p, float scale)
{
    p.r *= scale;
    p.g *= scale;
    p.b *= scale;
    p.a *= scale;
    return p;
}

static gdPremulPixelF lerp_pixel(gdPremulPixelF a, gdPremulPixelF b, float t)
{
    gdPremulPixelF r;
    t = clamp01(t);
    r.r = a.r + (b.r - a.r) * t;
    r.g = a.g + (b.g - a.g) * t;
    r.b = a.b + (b.b - a.b) * t;
    r.a = a.a + (b.a - a.a) * t;
    return clamp_pixel(r);
}

static float factor(FactorKind kind, float sa, float da)
{
    switch (kind) {
    case FACTOR_ZERO:
        return 0.0f;
    case FACTOR_ONE:
        return 1.0f;
    case FACTOR_SRC_ALPHA:
        return sa;
    case FACTOR_DST_ALPHA:
        return da;
    case FACTOR_ONE_MINUS_SRC_ALPHA:
        return 1.0f - sa;
    case FACTOR_ONE_MINUS_DST_ALPHA:
        return 1.0f - da;
    case FACTOR_SATURATE:
        return sa > 0.0f ? fminf(1.0f, (1.0f - da) / sa) : 0.0f;
    }
    return 0.0f;
}

static gdPremulPixelF composite_porter_duff(gdCompositeOperator op, gdPremulPixelF s,
                                            gdPremulPixelF d)
{
    const Factors f = porter_duff[op];
    const float sf = factor(f.src, s.a, d.a);
    const float df = factor(f.dst, s.a, d.a);
    gdPremulPixelF r;
    r.r = s.r * sf + d.r * df;
    r.g = s.g * sf + d.g * df;
    r.b = s.b * sf + d.b * df;
    r.a = s.a * sf + d.a * df;
    return clamp_pixel(r);
}

static float blend_channel(gdCompositeOperator op, float s, float d)
{
    switch (op) {
    case GD_OP_MULTIPLY:
        return s * d;
    case GD_OP_SCREEN:
        return s + d - s * d;
    case GD_OP_OVERLAY:
        return d <= 0.5f ? 2.0f * s * d : 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
    case GD_OP_DARKEN:
        return fminf(s, d);
    case GD_OP_LIGHTEN:
        return fmaxf(s, d);
    case GD_OP_COLOR_DODGE:
        return s >= 1.0f ? 1.0f : fminf(1.0f, d / (1.0f - s));
    case GD_OP_COLOR_BURN:
        return s <= 0.0f ? 0.0f : 1.0f - fminf(1.0f, (1.0f - d) / s);
    case GD_OP_HARD_LIGHT:
        return s <= 0.5f ? 2.0f * s * d : 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
    case GD_OP_SOFT_LIGHT:
        if (s <= 0.5f)
            return d - (1.0f - 2.0f * s) * d * (1.0f - d);
        else {
            float g = d <= 0.25f ? ((16.0f * d - 12.0f) * d + 4.0f) * d : sqrtf(d);
            return d + (2.0f * s - 1.0f) * (g - d);
        }
    case GD_OP_DIFFERENCE:
        return fabsf(d - s);
    case GD_OP_EXCLUSION:
        return s + d - 2.0f * s * d;
    default:
        return s;
    }
}

typedef struct {
    float r, g, b;
} StraightColor;

static float color_lum(StraightColor c) { return 0.30f * c.r + 0.59f * c.g + 0.11f * c.b; }

static float color_sat(StraightColor c)
{
    return fmaxf(c.r, fmaxf(c.g, c.b)) - fminf(c.r, fminf(c.g, c.b));
}

static StraightColor clip_color(StraightColor c)
{
    float l = color_lum(c);
    float n = fminf(c.r, fminf(c.g, c.b));
    float x = fmaxf(c.r, fmaxf(c.g, c.b));
    if (n < 0.0f) {
        float k = l / (l - n);
        c.r = l + (c.r - l) * k;
        c.g = l + (c.g - l) * k;
        c.b = l + (c.b - l) * k;
    }
    x = fmaxf(c.r, fmaxf(c.g, c.b));
    if (x > 1.0f) {
        float k = (1.0f - l) / (x - l);
        c.r = l + (c.r - l) * k;
        c.g = l + (c.g - l) * k;
        c.b = l + (c.b - l) * k;
    }
    return c;
}

static StraightColor set_lum(StraightColor c, float l)
{
    float d = l - color_lum(c);
    c.r += d;
    c.g += d;
    c.b += d;
    return clip_color(c);
}

static StraightColor set_sat(StraightColor c, float s)
{
    float *v[3] = {&c.r, &c.g, &c.b};
    float *tmp;
    if (*v[0] > *v[1]) {
        tmp = v[0];
        v[0] = v[1];
        v[1] = tmp;
    }
    if (*v[1] > *v[2]) {
        tmp = v[1];
        v[1] = v[2];
        v[2] = tmp;
    }
    if (*v[0] > *v[1]) {
        tmp = v[0];
        v[0] = v[1];
        v[1] = tmp;
    }
    if (*v[2] > *v[0]) {
        *v[1] = (*v[1] - *v[0]) * s / (*v[2] - *v[0]);
        *v[2] = s;
    } else {
        *v[1] = *v[2] = 0.0f;
    }
    *v[0] = 0.0f;
    return c;
}

static StraightColor blend_hsl(gdCompositeOperator op, StraightColor s, StraightColor d)
{
    switch (op) {
    case GD_OP_HSL_HUE:
        return set_lum(set_sat(s, color_sat(d)), color_lum(d));
    case GD_OP_HSL_SATURATION:
        return set_lum(set_sat(d, color_sat(s)), color_lum(d));
    case GD_OP_HSL_COLOR:
        return set_lum(s, color_lum(d));
    case GD_OP_HSL_LUMINOSITY:
        return set_lum(d, color_lum(s));
    default:
        return s;
    }
}

static gdPremulPixelF composite_blend(gdCompositeOperator op, gdPremulPixelF s, gdPremulPixelF d)
{
    StraightColor cs = {0, 0, 0}, cd = {0, 0, 0}, b;
    gdPremulPixelF r;
    if (s.a > 0.0f) {
        cs.r = s.r / s.a;
        cs.g = s.g / s.a;
        cs.b = s.b / s.a;
    }
    if (d.a > 0.0f) {
        cd.r = d.r / d.a;
        cd.g = d.g / d.a;
        cd.b = d.b / d.a;
    }
    if (op >= GD_OP_HSL_HUE)
        b = blend_hsl(op, cs, cd);
    else {
        b.r = blend_channel(op, cs.r, cd.r);
        b.g = blend_channel(op, cs.g, cd.g);
        b.b = blend_channel(op, cs.b, cd.b);
    }
    r.a = s.a + d.a * (1.0f - s.a);
    r.r = (1.0f - d.a) * s.r + (1.0f - s.a) * d.r + s.a * d.a * b.r;
    r.g = (1.0f - d.a) * s.g + (1.0f - s.a) * d.g + s.a * d.a * b.g;
    r.b = (1.0f - d.a) * s.b + (1.0f - s.a) * d.b + s.a * d.a * b.b;
    return clamp_pixel(r);
}

int gdCompositeOperatorIsValid(gdCompositeOperator op)
{
    return op >= GD_OP_CLEAR && op < GD_OP_COUNT;
}

int gdCompositeOperatorIsUnbounded(gdCompositeOperator op)
{
    return op == GD_OP_IN || op == GD_OP_OUT || op == GD_OP_DEST_IN || op == GD_OP_DEST_ATOP;
}

gdPremulPixelF gdCompositePixel(gdCompositeOperator op, gdPremulPixelF src, gdPremulPixelF dst,
                                float coverage)
{
    gdPremulPixelF result;
    coverage = clamp01(coverage);
    src = clamp_pixel(src);
    dst = clamp_pixel(dst);
    if (!gdCompositeOperatorIsValid(op))
        return dst;
    if (op == GD_OP_CLEAR || op == GD_OP_SOURCE) {
        result = composite_porter_duff(op, src, dst);
        return lerp_pixel(dst, result, coverage);
    }
    src = scale_pixel(src, coverage);
    if (op <= GD_OP_SATURATE)
        return composite_porter_duff(op, src, dst);
    return composite_blend(op, src, dst);
}

void gdCompositeSpan(gdCompositeOperator op, const gdPremulPixelF *src, ptrdiff_t src_stride,
                     gdPremulPixelF *dst, const float *coverage, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        float c = coverage ? coverage[i] : 1.0f;
        dst[i] = gdCompositePixel(op, *src, dst[i], c);
        src = (const gdPremulPixelF *)((const char *)src + src_stride);
    }
}

gdPremulPixelF gdCompositePixelFromArgb32(uint32_t p)
{
    gdPremulPixelF r;
    r.a = ((p >> 24) & 255) / 255.0f;
    r.r = ((p >> 16) & 255) / 255.0f;
    r.g = ((p >> 8) & 255) / 255.0f;
    r.b = (p & 255) / 255.0f;
    return clamp_pixel(r);
}

uint32_t gdCompositePixelToArgb32(gdPremulPixelF p)
{
    uint32_t a, r, g, b;
    p = clamp_pixel(p);
    a = (uint32_t)floorf(p.a * 255.0f + 0.5f);
    r = (uint32_t)floorf(p.r * 255.0f + 0.5f);
    g = (uint32_t)floorf(p.g * 255.0f + 0.5f);
    b = (uint32_t)floorf(p.b * 255.0f + 0.5f);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

gdPremulPixelF gdCompositePixelFromGd(int p)
{
    float a = (gdAlphaMax - gdTrueColorGetAlpha(p)) / (float)gdAlphaMax;
    gdPremulPixelF r;
    r.a = a;
    r.r = gdTrueColorGetRed(p) / 255.0f * a;
    r.g = gdTrueColorGetGreen(p) / 255.0f * a;
    r.b = gdTrueColorGetBlue(p) / 255.0f * a;
    return clamp_pixel(r);
}

int gdCompositePixelToGd(gdPremulPixelF p)
{
    int a, r = 0, g = 0, b = 0;
    p = clamp_pixel(p);
    a = (int)floorf((1.0f - p.a) * gdAlphaMax + 0.5f);
    if (p.a > 0.0f) {
        r = (int)floorf(p.r / p.a * 255.0f + 0.5f);
        g = (int)floorf(p.g / p.a * 255.0f + 0.5f);
        b = (int)floorf(p.b / p.a * 255.0f + 0.5f);
    }
    return gdTrueColorAlpha(r, g, b, a);
}

static int max_int(int a, int b) { return a > b ? a : b; }

static int min_int(int a, int b) { return a < b ? a : b; }

static int clamp_int32(int64_t value)
{
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    return (int)value;
}

static int clip_rect_to_bounds(const gdRect *rect, int width, int height, gdRect *out)
{
    int64_t x1, y1;

    if (rect == NULL) {
        out->x = 0;
        out->y = 0;
        out->width = width;
        out->height = height;
        return width > 0 && height > 0;
    }

    x1 = (int64_t)rect->x + rect->width;
    y1 = (int64_t)rect->y + rect->height;

    out->x = max_int(rect->x, 0);
    out->y = max_int(rect->y, 0);
    out->width = min_int(x1 > INT32_MAX ? INT32_MAX : (int)x1, width) - out->x;
    out->height = min_int(y1 > INT32_MAX ? INT32_MAX : (int)y1, height) - out->y;

    return out->width > 0 && out->height > 0;
}

static int gd_image_pixel_as_truecolor(const gdImagePtr im, int x, int y)
{
    if (im->trueColor) {
        return im->tpixels[y][x];
    } else {
        int c = im->pixels[y][x];
        return gdTrueColorAlpha(im->red[c], im->green[c], im->blue[c], im->alpha[c]);
    }
}

BGD_DECLARE(int)
gdImageComposite(gdImagePtr dst, const gdImagePtr src, int dst_x, int dst_y,
                 gdCompositeOperator op, double opacity, gdRectPtr src_region, gdRectPtr clip)
{
    gdRect src_full, src_bounds, dst_bounds, effect_bounds;
    const int transparent = gdTrueColorAlpha(0, 0, 0, gdAlphaTransparent);
    const int unbounded = gdCompositeOperatorIsUnbounded(op);
    int64_t placed_x = dst_x, placed_y = dst_y, placed_x1 = dst_x, placed_y1 = dst_y;
    int y, x;

    if (dst == NULL || src == NULL || !dst->trueColor || !gdCompositeOperatorIsValid(op) ||
        !isfinite(opacity) || opacity < 0.0 || opacity > 1.0) {
        return GD_FALSE;
    }

    src_full.x = 0;
    src_full.y = 0;
    src_full.width = gdImageSX(src);
    src_full.height = gdImageSY(src);
    if (src_region == NULL) {
        src_region = &src_full;
    }

    if (!clip_rect_to_bounds(clip, gdImageSX(dst), gdImageSY(dst), &dst_bounds)) {
        dst->saveAlphaFlag = 1;
        return GD_TRUE;
    }

    if (clip_rect_to_bounds(src_region, gdImageSX(src), gdImageSY(src), &src_bounds)) {
        placed_x = (int64_t)dst_x + ((int64_t)src_bounds.x - src_region->x);
        placed_y = (int64_t)dst_y + ((int64_t)src_bounds.y - src_region->y);
        placed_x1 = placed_x + src_bounds.width;
        placed_y1 = placed_y + src_bounds.height;
    }

    if (unbounded) {
        effect_bounds = dst_bounds;
    } else {
        int64_t effect_x = max_int(dst_bounds.x, clamp_int32(placed_x));
        int64_t effect_y = max_int(dst_bounds.y, clamp_int32(placed_y));
        int64_t effect_x1 = min_int(dst_bounds.x + dst_bounds.width, clamp_int32(placed_x1));
        int64_t effect_y1 = min_int(dst_bounds.y + dst_bounds.height, clamp_int32(placed_y1));

        if (effect_x1 <= effect_x || effect_y1 <= effect_y) {
            dst->saveAlphaFlag = 1;
            return GD_TRUE;
        }
        effect_bounds.x = (int)effect_x;
        effect_bounds.y = (int)effect_y;
        effect_bounds.width = (int)(effect_x1 - effect_x);
        effect_bounds.height = (int)(effect_y1 - effect_y);
    }

    for (y = effect_bounds.y; y < effect_bounds.y + effect_bounds.height; y++) {
        int *dst_row = dst->tpixels[y];

        for (x = effect_bounds.x; x < effect_bounds.x + effect_bounds.width; x++) {
            int src_color = transparent;
            int64_t src_x = (int64_t)src_region->x + (x - dst_x);
            int64_t src_y = (int64_t)src_region->y + (y - dst_y);
            gdPremulPixelF src_pixel, dst_pixel;

            if (src_x >= src_bounds.x && src_x < src_bounds.x + src_bounds.width &&
                src_y >= src_bounds.y && src_y < src_bounds.y + src_bounds.height) {
                src_color = gd_image_pixel_as_truecolor(src, (int)src_x, (int)src_y);
            }

            src_pixel = gdCompositePixelFromGd(src_color);
            dst_pixel = gdCompositePixelFromGd(dst_row[x]);
            dst_row[x] = gdCompositePixelToGd(
                gdCompositePixel(op, src_pixel, dst_pixel, (float)opacity));
        }
    }

    dst->saveAlphaFlag = 1;
    return GD_TRUE;
}
