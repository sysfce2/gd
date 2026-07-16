#include "gd_vector2d.h"
#include "gdfonts.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 320
#define HEIGHT 180
#define FRAMES 60
#define NSTARS 80

typedef struct {
    float x, y, speed, brightness;
} Star;

static Star stars[NSTARS];

static int clamp_u8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

static double phase(int frame, double speed) { return fmod((double)frame / FRAMES * speed, 1.0); }

static double tw(int frame, double speed) { return phase(frame, speed) * 2.0 * M_PI; }

static void init_stars(void)
{
    unsigned int rng = 0xdeadbeef;
    int i;

    for (i = 0; i < NSTARS; i++) {
        rng = rng * 1664525u + 1013904223u;
        stars[i].x = (rng & 0xffff) / 65535.0f * WIDTH;
        rng = rng * 1664525u + 1013904223u;
        stars[i].y = (rng & 0xffff) / 65535.0f * HEIGHT;
        rng = rng * 1664525u + 1013904223u;
        stars[i].speed = 0.3f + ((rng >> 16) % 3) * 0.5f;
        rng = rng * 1664525u + 1013904223u;
        stars[i].brightness = 0.4f + (rng & 0xff) / 255.0f * 0.6f;
    }
}

/* The procedural background is intentionally identical to jxl_create_anim. */
static void draw_plasma(gdImagePtr image, int frame)
{
    double t = (double)frame / FRAMES * 2.0 * M_PI;
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            double fx = x / (double)WIDTH;
            double fy = y / (double)HEIGHT;
            double v = sin(fx * 6.0 + t);

            v += sin(fy * 4.0 + t * 1.3);
            v += sin((fx + fy) * 5.0 + t * 0.7);
            v += sin(sqrt((fx - 0.5 + 0.3 * sin(t)) * (fx - 0.5 + 0.3 * sin(t)) +
                          (fy - 0.5 + 0.3 * cos(t)) * (fy - 0.5 + 0.3 * cos(t))) *
                     8.0);
            v = (v + 4.0) / 8.0;
            gdImageSetPixel(image, x, y,
                            gdTrueColor(clamp_u8((int)(18 + v * 40)), clamp_u8((int)(10 + v * 25)),
                                        clamp_u8((int)(60 + v * 80))));
        }
    }
}

static void draw_stars(gdContextPtr context, int frame)
{
    int i;

    for (i = 0; i < NSTARS; i++) {
        double scroll = stars[i].speed * WIDTH * ((double)frame / FRAMES);
        double x = fmod(stars[i].x + scroll, WIDTH);
        double twinkle = 0.7 + 0.3 * sin(tw(frame, stars[i].speed * 2.0) + stars[i].x * 0.05);
        double brightness = stars[i].brightness * (220.0 / 255.0) * twinkle;

        gdContextNewPath(context);
        gdContextArc(context, x, stars[i].y, 0.75, 0, 2.0 * M_PI);
        gdContextSetSourceRgba(context, brightness, brightness, brightness + 20.0 / 255.0, 1.0);
        gdContextFill(context);
    }
}

static void draw_scanlines(gdContextPtr context, int frame)
{
    double shift = (double)frame / FRAMES * WIDTH;
    int s;

    gdContextNewPath(context);
    for (s = 0; s < WIDTH + HEIGHT; s += 22) {
        double x0 = fmod(s - shift, WIDTH);
        if (x0 < 0)
            x0 += WIDTH;
        gdContextMoveTo(context, x0, 0);
        gdContextLineTo(context, x0 - HEIGHT, HEIGHT);
    }
    gdContextSetSourceRgba(context, 1, 1, 1, 0.15);
    gdContextSetLineWidth(context, 1.0);
    gdContextStroke(context);
}

static void fill_circle(gdContextPtr context, double x, double y, double radius, double red,
                        double green, double blue, double alpha)
{
    gdContextNewPath(context);
    gdContextArc(context, x, y, radius, 0, 2.0 * M_PI);
    gdContextSetSourceRgba(context, red, green, blue, alpha);
    gdContextFill(context);
}

static void draw_orbs(gdContextPtr context, int frame)
{
    double t = tw(frame, 1.0);
    double cx1 = WIDTH / 2.0 + cos(t) * (WIDTH / 2.0 - 50);
    double cy1 = HEIGHT / 2.0 + sin(t) * (HEIGHT / 2.0 - 30);
    double cx2 = WIDTH / 2.0 + cos(t + M_PI) * (WIDTH / 2.0 - 60);
    double cy2 = HEIGHT / 2.0 + sin(t + M_PI) * (HEIGHT / 2.0 - 25);
    double r1 = 32 + 8 * sin(tw(frame, 2.0));
    double r2 = 26 + 6 * sin(tw(frame, 2.0) + M_PI);
    int ring;

    fill_circle(context, cx1, cy1, r1, 80.0 / 255.0, 220.0 / 255.0, 1, 0.70);
    fill_circle(context, cx2, cy2, r2, 1, 200.0 / 255.0, 80.0 / 255.0, 0.67);

    for (ring = 0; ring < 4; ring++) {
        double rphase = fmod(tw(frame, 1.5) / (2 * M_PI) + ring * 0.25, 1.0);
        double radius = rphase * 70;
        double alpha = (1.0 - rphase) * (100.0 / 127.0);

        gdContextNewPath(context);
        gdContextArc(context, cx1, cy1, radius, 0, 2.0 * M_PI);
        gdContextArc(context, cx2, cy2, radius + 5, 0, 2.0 * M_PI);
        gdContextSetSourceRgba(context, 180.0 / 255.0, 240.0 / 255.0, 1, alpha);
        gdContextSetLineWidth(context, 1.0);
        gdContextStroke(context);
    }
}

static void draw_hud_geometry(gdContextPtr context)
{
    gdContextNewPath(context);
    gdContextRectangle(context, 0, HEIGHT - 26, WIDTH, 26);
    gdContextSetSourceRgba(context, 0, 0, 0, 47.0 / 127.0);
    gdContextFill(context);
}

#define TICKER                                                                                     \
    "  libgd JPEG XL animation  \xb7  seamless infinite loop  \xb7  plasma + "                     \
    "starfield + orbs  \xb7  "

/* Vector2D does not yet provide text rendering, so this remains identical. */
static void draw_ticker(gdImagePtr image, int frame)
{
    int char_width = gdFontGetSmall()->w;
    int ticker_length = (int)strlen(TICKER);
    int total_width = ticker_length * char_width;
    int offset = (int)((double)frame / FRAMES * total_width) % total_width;
    int x;

    for (x = -offset; x < WIDTH; x += total_width) {
        gdImageString(image, gdFontGetSmall(), x, HEIGHT - 18, (unsigned char *)TICKER,
                      gdTrueColorAlpha(180, 230, 255, 20));
    }
}

static gdImagePtr make_frame(int frame)
{
    gdImagePtr image = gdImageCreateTrueColor(WIDTH, HEIGHT);
    gdContextPtr context;

    if (!image)
        return NULL;
    gdImageAlphaBlending(image, 0);
    gdImageSaveAlpha(image, 1);
    draw_plasma(image, frame);

    context = gdContextCreateForImage(image);
    if (!context) {
        gdImageDestroy(image);
        return NULL;
    }
    draw_stars(context, frame);
    draw_scanlines(context, frame);
    draw_orbs(context, frame);
    draw_hud_geometry(context);
    gdContextDestroy(context);

    gdImageAlphaBlending(image, 1);
    draw_ticker(image, frame);
    return image;
}

int main(int argc, char **argv)
{
    FILE *output;
    gdJxlWriteOptions options;
    gdJxlWritePtr writer;
    int lossless;
    float distance;
    int i;

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s output.jxl [lossless|lossy] [distance]\n", argv[0]);
        return 1;
    }
    lossless = argc >= 3 && strcmp(argv[2], "lossless") == 0;
    distance = argc >= 4 ? (float)atof(argv[3]) : 1.0f;
    init_stars();

    output = fopen(argv[1], "wb");
    if (!output) {
        fprintf(stderr, "cannot create %s\n", argv[1]);
        return 1;
    }

    gdJxlWriteOptionsInit(&options);
    options.canvasWidth = WIDTH;
    options.canvasHeight = HEIGHT;
    options.lossless = lossless;
    options.distance = distance;
    options.effort = 7;

    writer = gdJxlWriteOpen(output, &options);
    if (!writer) {
        fprintf(stderr, "cannot create JXL animation writer\n");
        fclose(output);
        return 1;
    }

    for (i = 0; i < FRAMES; i++) {
        gdImagePtr frame = make_frame(i);
        if (!frame || !gdJxlWriteAddImage(writer, frame, 40)) {
            fprintf(stderr, "cannot create or add frame %d\n", i);
            gdImageDestroy(frame);
            gdJxlWriteClose(writer);
            fclose(output);
            return 1;
        }
        gdImageDestroy(frame);
        fprintf(stderr, "\rframe %d/%d", i + 1, FRAMES);
    }
    fprintf(stderr, "\n");
    gdJxlWriteClose(writer);
    fclose(output);
    printf("wrote %s: %d Vector2D frames, %dx%d, %s distance=%.2f\n", argv[1], FRAMES, WIDTH,
           HEIGHT, lossless ? "lossless" : "lossy", distance);
    return 0;
}
