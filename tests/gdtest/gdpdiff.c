#include "gd.h"
#include "gdtest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gdImagePtr load_image(const char *path) {
	FILE *f;
	gdImagePtr im = NULL;
	const char *ext = strrchr(path, '.');

	if (!ext) {
		fprintf(stderr, "Cannot determine image type for: %s\n", path);
		return NULL;
	}
	ext++;

	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Cannot open: %s\n", path);
		return NULL;
	}

	if (strcasecmp(ext, "png") == 0) {
		im = gdImageCreateFromPng(f);
	} else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
		im = gdImageCreateFromJpeg(f);
	} else if (strcasecmp(ext, "gif") == 0) {
		im = gdImageCreateFromGif(f);
	} else if (strcasecmp(ext, "webp") == 0) {
		im = gdImageCreateFromWebp(f);
	} else {
		fprintf(stderr, "Unsupported format: %s\n", ext);
	}

	fclose(f);

	if (!im) {
		fprintf(stderr, "Failed to decode: %s\n", path);
	}
	return im;
}

static int save_image(gdImagePtr im, const char *path) {
	FILE *f;
	const char *ext = strrchr(path, '.');

	if (!ext) {
		fprintf(stderr, "Cannot determine output image type for: %s\n", path);
		return 0;
	}
	ext++;

	f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "Cannot write: %s\n", path);
		return 0;
	}

	if (strcasecmp(ext, "png") == 0) {
		gdImagePng(im, f);
	} else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
		gdImageJpeg(im, f, 95);
	} else {
		fprintf(stderr, "Unsupported output format: %s, saving as PNG\n", ext);
		gdImagePng(im, f);
	}

	fclose(f);
	return 1;
}

static void usage(const char *prog) {
	fprintf(stderr,
			"Usage: %s [options] <image1> <image2>\n"
			"\n"
			"Options:\n"
			"  -t <threshold>   Perceptual threshold 0.0-1.0 (default: 0.1)\n"
			"  -o <diff.png>    Save diff image to file\n"
			"  -q               Quiet — only output the count\n"
			"  -h               Show this help\n"
			"\n"
			"Exit codes:\n"
			"  0  Images are identical within threshold\n"
			"  1  Images differ\n"
			"  2  Error\n",
			prog);
}

int main(int argc, char *argv[]) {
	const char *path1 = NULL;
	const char *path2 = NULL;
	const char *diffpath = NULL;
	double threshold = 0.1;
	int quiet = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-q") == 0) {
			quiet = 1;
		} else if (strcmp(argv[i], "-t") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "Missing value for -t\n");
				return 2;
			}
			threshold = atof(argv[i]);
			if (threshold < 0.0 || threshold > 1.0) {
				fprintf(stderr, "Threshold must be between 0.0 and 1.0\n");
				return 2;
			}
		} else if (strcmp(argv[i], "-o") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "Missing value for -o\n");
				return 2;
			}
			diffpath = argv[i];
		} else if (!path1) {
			path1 = argv[i];
		} else if (!path2) {
			path2 = argv[i];
		} else {
			fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
			usage(argv[0]);
			return 2;
		}
	}

	if (!path1 || !path2) {
		usage(argv[0]);
		return 2;
	}

	gdImagePtr img1 = load_image(path1);
	gdImagePtr img2 = load_image(path2);

	if (!img1 || !img2) {
		if (img1)
			gdImageDestroy(img1);
		if (img2)
			gdImageDestroy(img2);
		return 2;
	}

	if (gdImageSX(img1) != gdImageSX(img2) ||
		gdImageSY(img1) != gdImageSY(img2)) {
		fprintf(stderr, "Image dimensions differ: %dx%d vs %dx%d\n",
				gdImageSX(img1), gdImageSY(img1), gdImageSX(img2),
				gdImageSY(img2));
		gdImageDestroy(img1);
		gdImageDestroy(img2);
		return 2;
	}

	gdImagePtr buf_diff = NULL;
	gdImagePerceptualDiffResult result;
	if (!gdImagePerceptualDiff(img1, img2, threshold, NULL,
								diffpath ? &buf_diff : NULL, &result)) {
		fprintf(stderr, "Failed to compare images\n");
		gdImageDestroy(img1);
		gdImageDestroy(img2);
		return 2;
	}

	if (!quiet) {
		printf("Pixels changed: %d / %d\n", result.pixels_changed,
			   gdImageSX(img1) * gdImageSY(img1));
	} else {
		printf("%d\n", result.pixels_changed);
	}

	if (buf_diff) {
		if (result.pixels_changed > 0) {
			if (!save_image(buf_diff, diffpath)) {
				gdImageDestroy(img1);
				gdImageDestroy(img2);
				gdImageDestroy(buf_diff);
				return 2;
			}
			if (!quiet) {
				printf("Diff image saved to: %s\n", diffpath);
			}
		} else if (!quiet) {
			printf("No differences, diff image not saved.\n");
		}
		gdImageDestroy(buf_diff);
	}

	gdImageDestroy(img1);
	gdImageDestroy(img2);

	return result.pixels_changed > 0 ? 1 : 0;
}
