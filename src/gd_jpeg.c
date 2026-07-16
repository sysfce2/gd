/*
 * gd_jpeg.c: Read and write JPEG (JFIF) format image files using the
 * gd graphics library (https://libgd.github.io).
 *
 * This software is based in part on the work of the Independent JPEG
 * Group.  For more information on the IJG JPEG software (and JPEG
 * documentation, etc.), see ftp://ftp.uu.net/graphics/jpeg/.
 *
 * NOTE: IJG 12-bit JSAMPLE (BITS_IN_JSAMPLE == 12) mode is not
 * supported at all on read in gd 2.0, and is not supported on write
 * except for palette images, which is sort of pointless (TBB). Even that
 * has never been tested according to DB.
 *
 * Copyright 2000 Doug Becker, mailto:thebeckers@home.com
 *
 * Modification 4/18/00 TBB: JPEG_DEBUG rather than just DEBUG,
 * so VC++ builds don't spew to standard output, causing
 * major CGI brain damage
 *
 * 2.0.10: more efficient gdImageCreateFromJpegCtx, thanks to
 * Christian Aberger
 */

/**
 * File: JPEG IO
 *
 * Read and write JPEG images.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "gd.h"
#include "gd_errors.h"
#include "gd_intern.h"
/* TBB: move this up so include files are not brought in */
/* JCE: arrange HAVE_LIBJPEG so that it can be set in gd.h */
#ifdef HAVE_LIBJPEG
#include "gdhelpers.h"

#if defined(_WIN32) && defined(__MINGW32__)
#define HAVE_BOOLEAN
#endif

/* 1.8.1: remove dependency on jinclude.h */
#include "jerror.h"
#include "jpeglib.h"

static const char *const GD_JPEG_VERSION = "1.0";

typedef struct _jmpbuf_wrapper {
    jmp_buf jmpbuf;
    int ignore_warning;
} jmpbuf_wrapper;

static void jpeg_gdIOCtx_src(j_decompress_ptr cinfo, gdIOCtx *infile);

static void jpeg_emit_message(j_common_ptr jpeg_info, int level)
{
    char message[JMSG_LENGTH_MAX];
    jmpbuf_wrapper *jmpbufw;
    int ignore_warning = 0;

    jmpbufw = (jmpbuf_wrapper *)jpeg_info->client_data;

    if (jmpbufw != 0) {
        ignore_warning = jmpbufw->ignore_warning;
    }

    (jpeg_info->err->format_message)(jpeg_info, message);

    /* It is a warning message */
    if (level < 0) {
        /* display only the 1st warning, as would do a default libjpeg
         * unless strace_level >= 3
         */
        if ((jpeg_info->err->num_warnings == 0) || (jpeg_info->err->trace_level >= 3)) {
            if (!ignore_warning) {
                gd_error("gd-jpeg, libjpeg: recoverable error: %s\n", message);
            }
        }

        jpeg_info->err->num_warnings++;
    } else {
        /* strace msg, Show it if trace_level >= level. */
        if (jpeg_info->err->trace_level >= level) {
            if (!ignore_warning) {
                gd_error("gd-jpeg, libjpeg: strace message: %s\n", message);
            }
        }
    }
}

/* Called by the IJG JPEG library upon encountering a fatal error */
static void fatal_jpeg_error(j_common_ptr cinfo)
{
    jmpbuf_wrapper *jmpbufw;
    char buffer[JMSG_LENGTH_MAX];

    (*cinfo->err->format_message)(cinfo, buffer);
    gd_error_ex(GD_WARNING, "gd-jpeg: JPEG library reports unrecoverable error: %s", buffer);

    jmpbufw = (jmpbuf_wrapper *)cinfo->client_data;
    jpeg_destroy(cinfo);

    if (jmpbufw != 0) {
        longjmp(jmpbufw->jmpbuf, 1);
        gd_error_ex(GD_ERROR, "gd-jpeg: EXTREMELY fatal error: longjmp "
                              "returned control; terminating\n");
    } else {
        gd_error_ex(GD_ERROR, "gd-jpeg: EXTREMELY fatal error: jmpbuf "
                              "unrecoverable; terminating\n");
    }

    exit(99);
}

BGD_DECLARE(const char *) gdJpegGetVersionString()
{
    switch (JPEG_LIB_VERSION) {
    case 62:
        return "6b";
        break;

    case 70:
        return "7";
        break;

    case 80:
        return "8";
        break;

    case 90:
        return "9 compatible";
        break;

    case 100:
        return "10 compatible";
        break;

    default:
        return "unknown";
    }
}

BGD_DECLARE(void) gdJpegInfoInit(gdJpegInfo *info)
{
    if (info == NULL) {
        return;
    }
    memset(info, 0, sizeof(*info));
    info->width = 0;
    info->height = 0;
    info->bits_per_sample = 0;
    info->components = 0;
    info->color_space = GD_JPEG_COLOR_SPACE_UNKNOWN;
    info->progressive = 0;
    info->density_unit = GD_JPEG_DENSITY_UNIT_NONE;
    info->x_density = -1;
    info->y_density = -1;
}

BGD_DECLARE(void) gdJpegReadOptionsInit(gdJpegReadOptions *options)
{
    if (options == NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->ignore_warning = 1;
    options->scale_num = 1;
    options->scale_denom = 1;
    options->dct_method = GD_JPEG_DCT_DEFAULT;
}

static int gdJpegMapColorSpace(J_COLOR_SPACE color_space)
{
    switch (color_space) {
    case JCS_GRAYSCALE:
        return GD_JPEG_COLOR_SPACE_GRAYSCALE;
    case JCS_RGB:
        return GD_JPEG_COLOR_SPACE_RGB;
    case JCS_YCbCr:
        return GD_JPEG_COLOR_SPACE_YCBCR;
    case JCS_CMYK:
        return GD_JPEG_COLOR_SPACE_CMYK;
    case JCS_YCCK:
        return GD_JPEG_COLOR_SPACE_YCCK;
    case JCS_UNKNOWN:
    default:
        return GD_JPEG_COLOR_SPACE_UNKNOWN;
    }
}

static int gdJpegMarkerStartsWith(jpeg_saved_marker_ptr marker, const unsigned char *prefix,
                                  size_t prefix_size);

static int gdJpegInfoCollectMarkers(j_decompress_ptr cinfo, gdJpegInfo *info)
{
    static const unsigned char exif_signature[] = {'E', 'x', 'i', 'f', '\0', '\0'};
    static const unsigned char xmp_signature[] = "http://ns.adobe.com/xap/1.0/";
    static const unsigned char iptc_signature[] = "Photoshop 3.0";
    static const unsigned char icc_signature[] = "ICC_PROFILE";
    jpeg_saved_marker_ptr marker;

    for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
        if (marker->marker == JPEG_APP0 + 1 &&
            gdJpegMarkerStartsWith(marker, exif_signature, sizeof(exif_signature))) {
            info->has_exif = 1;
        } else if (marker->marker == JPEG_APP0 + 1 &&
                   gdJpegMarkerStartsWith(marker, xmp_signature, sizeof(xmp_signature))) {
            info->has_xmp = 1;
        } else if (marker->marker == JPEG_APP0 + 2 &&
                   gdJpegMarkerStartsWith(marker, icc_signature, sizeof(icc_signature) - 1)) {
            info->has_icc = 1;
        } else if (marker->marker == JPEG_APP0 + 13 &&
                   gdJpegMarkerStartsWith(marker, iptc_signature, sizeof(iptc_signature))) {
            info->has_iptc = 1;
        }
    }

    return 0;
}

BGD_DECLARE(int) gdJpegGetInfoCtx(gdIOCtxPtr infile, gdJpegInfo *info)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jmpbuf_wrapper jmpbufw;
    int retval;

    if (infile == NULL || info == NULL) {
        return 1;
    }

    gdJpegInfoInit(info);

    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));
    memset(&jmpbufw, 0, sizeof(jmpbufw));
    jmpbufw.ignore_warning = 1;

    cinfo.err = jpeg_std_error(&jerr);
    cinfo.client_data = &jmpbufw;
    cinfo.err->emit_message = jpeg_emit_message;

    if (setjmp(jmpbufw.jmpbuf) != 0) {
        jpeg_destroy_decompress(&cinfo);
        return 1;
    }

    cinfo.err->error_exit = fatal_jpeg_error;

    jpeg_create_decompress(&cinfo);
    jpeg_gdIOCtx_src(&cinfo, infile);

    jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF);
    jpeg_save_markers(&cinfo, JPEG_APP0 + 2, 0xFFFF);
    jpeg_save_markers(&cinfo, JPEG_APP0 + 13, 0xFFFF);

    retval = jpeg_read_header(&cinfo, TRUE);
    if (retval != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return 1;
    }

    if (cinfo.image_width > INT_MAX || cinfo.image_height > INT_MAX ||
        cinfo.data_precision > INT_MAX || cinfo.num_components > INT_MAX) {
        jpeg_destroy_decompress(&cinfo);
        return 1;
    }

    info->width = (int)cinfo.image_width;
    info->height = (int)cinfo.image_height;
    info->bits_per_sample = (int)cinfo.data_precision;
    info->components = cinfo.num_components;
    info->color_space = gdJpegMapColorSpace(cinfo.jpeg_color_space);
    info->progressive = cinfo.progressive_mode != 0;
    info->density_unit = cinfo.density_unit;
    info->x_density = (int)cinfo.X_density;
    info->y_density = (int)cinfo.Y_density;
    gdJpegInfoCollectMarkers(&cinfo, info);

    jpeg_destroy_decompress(&cinfo);
    return 0;
}

BGD_DECLARE(int) gdJpegGetInfo(FILE *infile, gdJpegInfo *info)
{
    gdIOCtx *in;
    int result;

    in = gdNewFileCtx(infile);
    if (in == NULL) {
        return 1;
    }
    result = gdJpegGetInfoCtx(in, info);
    in->gd_free(in);
    return result;
}

BGD_DECLARE(int) gdJpegGetInfoPtr(int size, const void *data, gdJpegInfo *info)
{
    gdIOCtx *in;
    int result;

    if (size <= 0 || data == NULL) {
        return 1;
    }
    in = gdNewDynamicCtxEx(size, (void *)data, 0);
    if (in == NULL) {
        return 1;
    }
    result = gdJpegGetInfoCtx(in, info);
    in->gd_free(in);
    return result;
}

static int _gdImageJpegCtx(gdImagePtr im, gdIOCtx *outfile, int quality,
                           const gdImageMetadata *metadata, int force_no_subsampling,
                           int progressive);

BGD_DECLARE(void) gdImageJpeg(gdImagePtr im, FILE *outFile, int quality)
{
    gdIOCtx *out = gdNewFileCtx(outFile);
    if (out == NULL)
        return;
    gdImageJpegCtx(im, out, quality);
    out->gd_free(out);
}

BGD_DECLARE(void *) gdImageJpegPtr(gdImagePtr im, int *size, int quality)
{
    void *rv;
    gdIOCtx *out = gdNewDynamicCtx(2048, NULL);
    if (out == NULL)
        return NULL;
    if (!_gdImageJpegCtx(im, out, quality, NULL, 0, -1)) {
        rv = gdDPExtractData(out, size);
    } else {
        rv = NULL;
    }
    out->gd_free(out);
    return rv;
}

BGD_DECLARE(void *)
gdImageJpegPtrWithMetadata(gdImagePtr im, int *size, int quality, const gdImageMetadata *metadata)
{
    void *rv;
    gdIOCtx *out = gdNewDynamicCtx(2048, NULL);
    if (out == NULL)
        return NULL;
    if (!_gdImageJpegCtx(im, out, quality, metadata, 0, -1)) {
        rv = gdDPExtractData(out, size);
    } else {
        rv = NULL;
    }
    out->gd_free(out);
    return rv;
}

void *gdImageJpegPtrWithMetadataNoSubsampling(gdImagePtr im, int *size, int quality,
                                              const gdImageMetadata *metadata)
{
    void *rv;
    gdIOCtx *out = gdNewDynamicCtx(2048, NULL);
    if (out == NULL)
        return NULL;
    if (!_gdImageJpegCtx(im, out, quality, metadata, 1, -1)) {
        rv = gdDPExtractData(out, size);
    } else {
        rv = NULL;
    }
    out->gd_free(out);
    return rv;
}

BGD_DECLARE(void) gdJpegWriteOptionsInit(gdJpegWriteOptions *options)
{
    if (options == NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->quality = -1;
    options->progressive = 0;
    options->force_no_subsampling = 0;
    options->metadata = NULL;
}

BGD_DECLARE(int)
gdImageJpegWithOptions(gdImagePtr im, FILE *outFile, const gdJpegWriteOptions *options)
{
    gdIOCtx *out;
    int result;

    out = gdNewFileCtx(outFile);
    if (out == NULL) {
        return 1;
    }
    result = gdImageJpegCtxWithOptions(im, out, options);
    out->gd_free(out);
    return result;
}

BGD_DECLARE(int)
gdImageJpegCtxWithOptions(gdImagePtr im, gdIOCtx *outfile, const gdJpegWriteOptions *options)
{
    gdJpegWriteOptions default_options;

    if (options == NULL) {
        gdJpegWriteOptionsInit(&default_options);
        options = &default_options;
    }

    return _gdImageJpegCtx(im, outfile, options->quality, options->metadata,
                           options->force_no_subsampling, options->progressive);
}

BGD_DECLARE(void *)
gdImageJpegPtrWithOptions(gdImagePtr im, int *size, const gdJpegWriteOptions *options)
{
    void *rv;
    gdIOCtx *out = gdNewDynamicCtx(2048, NULL);
    if (out == NULL) {
        return NULL;
    }
    if (!gdImageJpegCtxWithOptions(im, out, options)) {
        rv = gdDPExtractData(out, size);
    } else {
        rv = NULL;
    }
    out->gd_free(out);
    return rv;
}

static void jpeg_gdIOCtx_dest(j_compress_ptr cinfo, gdIOCtx *outfile);

BGD_DECLARE(void) gdImageJpegCtx(gdImagePtr im, gdIOCtx *outfile, int quality)
{
    _gdImageJpegCtx(im, outfile, quality, NULL, 0, -1);
}

BGD_DECLARE(void)
gdImageJpegCtxWithMetadata(gdImagePtr im, gdIOCtx *outfile, int quality,
                           const gdImageMetadata *metadata)
{
    _gdImageJpegCtx(im, outfile, quality, metadata, 0, -1);
}

/* returns 0 on success, 1 on failure */
static int gdJpegWriteAppMarker(j_compress_ptr cinfo, int marker, const unsigned char *data,
                                size_t size)
{
    if (data == NULL && size != 0) {
        return 1;
    }
    if (size > 65533) {
        return 1;
    }
    jpeg_write_marker(cinfo, marker, data, (unsigned int)size);
    return 0;
}

static int gdJpegWriteIccProfile(j_compress_ptr cinfo, const unsigned char *data, size_t size)
{
    static const unsigned char icc_signature[] = "ICC_PROFILE";
    unsigned char *segment;
    size_t offset = 0;
    size_t max_payload = 65533 - 14;
    int segment_count;
    int segment_index;

    if (data == NULL && size != 0) {
        return 1;
    }
    if (size == 0) {
        return 0;
    }
    if (size > max_payload * 255) {
        return 1;
    }

    segment_count = (int)((size + max_payload - 1) / max_payload);
    segment = (unsigned char *)gdMalloc(65533);
    if (segment == NULL) {
        return 1;
    }
    memcpy(segment, icc_signature, 12);

    for (segment_index = 1; segment_index <= segment_count; segment_index++) {
        size_t chunk_size = size - offset;
        if (chunk_size > max_payload) {
            chunk_size = max_payload;
        }
        segment[12] = (unsigned char)segment_index;
        segment[13] = (unsigned char)segment_count;
        memcpy(segment + 14, data + offset, chunk_size);
        jpeg_write_marker(cinfo, JPEG_APP0 + 2, segment, (unsigned int)(chunk_size + 14));
        offset += chunk_size;
    }

    gdFree(segment);
    return 0;
}

static int gdJpegWriteMetadata(j_compress_ptr cinfo, const gdImageMetadata *metadata)
{
    const unsigned char *data;
    size_t size;

    if (metadata == NULL) {
        return 0;
    }

    data = gdImageMetadataGetProfile(metadata, "exif", &size);
    if (data != NULL && gdJpegWriteAppMarker(cinfo, JPEG_APP0 + 1, data, size)) {
        return 1;
    }
    data = gdImageMetadataGetProfile(metadata, "xmp", &size);
    if (data != NULL && gdJpegWriteAppMarker(cinfo, JPEG_APP0 + 1, data, size)) {
        return 1;
    }
    data = gdImageMetadataGetProfile(metadata, "icc", &size);
    if (data != NULL && gdJpegWriteIccProfile(cinfo, data, size)) {
        return 1;
    }
    data = gdImageMetadataGetProfile(metadata, "iptc", &size);
    if (data != NULL && gdJpegWriteAppMarker(cinfo, JPEG_APP0 + 13, data, size)) {
        return 1;
    }

    return 0;
}

static int _gdImageJpegCtx(gdImagePtr im, gdIOCtx *outfile, int quality,
                           const gdImageMetadata *metadata, int force_no_subsampling,
                           int progressive)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    int i, j, jidx;
    /* volatile so we can gdFree it on return from longjmp */
    volatile JSAMPROW row = 0;
    JSAMPROW rowptr[1];
    jmpbuf_wrapper jmpbufw;
    JDIMENSION nlines;
    char comment[255];

#ifdef JPEG_DEBUG
    gd_error_ex(GD_DEBUG, "gd-jpeg: gd JPEG version %s\n", GD_JPEG_VERSION);
    gd_error_ex(GD_DEBUG, "gd-jpeg: JPEG library version %d, %d-bit sample values\n",
                JPEG_LIB_VERSION, BITS_IN_JSAMPLE);
    if (!im->trueColor) {
        for (i = 0; i < im->colorsTotal; i++) {
            if (!im->open[i]) {
                gd_error_ex(GD_DEBUG, "gd-jpeg: gd colormap index %d: (%d, %d, %d)\n", i,
                            im->red[i], im->green[i], im->blue[i]);
            }
        }
    }
#endif /* JPEG_DEBUG */

    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));

    cinfo.err = jpeg_std_error(&jerr);
    cinfo.client_data = &jmpbufw;

    if (setjmp(jmpbufw.jmpbuf) != 0) {
        /* we're here courtesy of longjmp */
        if (row) {
            gdFree(row);
        }
        return 1;
    }

    cinfo.err->emit_message = jpeg_emit_message;
    cinfo.err->error_exit = fatal_jpeg_error;

    jpeg_create_compress(&cinfo);

    cinfo.image_width = im->sx;
    cinfo.image_height = im->sy;
    cinfo.input_components = 3;     /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; /* colorspace of input image */

    jpeg_set_defaults(&cinfo);

    cinfo.density_unit = 1;
    cinfo.X_density = im->res_x;
    cinfo.Y_density = im->res_y;

    if (quality >= 0) {
        jpeg_set_quality(&cinfo, quality, TRUE);
    }
    if (force_no_subsampling || quality >= 90) {
        for (i = 0; i < cinfo.num_components; i++) {
            cinfo.comp_info[i].h_samp_factor = 1;
            cinfo.comp_info[i].v_samp_factor = 1;
        }
    }

    /* If user requests interlace, translate that to progressive JPEG */
    if ((progressive >= 0 && progressive) || (progressive < 0 && gdImageGetInterlaced(im))) {
#ifdef JPEG_DEBUG
        gd_error_ex(GD_DEBUG, "gd-jpeg: interlace set, outputting progressive JPEG image\n");
#endif
        jpeg_simple_progression(&cinfo);
    }

    jpeg_gdIOCtx_dest(&cinfo, outfile);

    row = (JSAMPROW)gdCalloc(1, cinfo.image_width * cinfo.input_components * sizeof(JSAMPLE));
    if (row == 0) {
        gd_error("gd-jpeg: error: unable to allocate JPEG row structure: "
                 "gdCalloc returns NULL\n");
        jpeg_destroy_compress(&cinfo);
        return 1;
    }

    rowptr[0] = row;

    jpeg_start_compress(&cinfo, TRUE);

    if (gdJpegWriteMetadata(&cinfo, metadata)) {
        gd_error("gd-jpeg: error: unable to write metadata\n");
        goto error;
    }

    sprintf(comment, "CREATOR: gd-jpeg v%s (using IJG JPEG v%d),", GD_JPEG_VERSION,
            JPEG_LIB_VERSION);

    if (quality >= 0) {
        sprintf(comment + strlen(comment), " quality = %d\n", quality);
    } else {
        strcat(comment + strlen(comment), " default quality\n");
    }

    jpeg_write_marker(&cinfo, JPEG_COM, (unsigned char *)comment, (unsigned int)strlen(comment));

    if (im->trueColor) {
#if BITS_IN_JSAMPLE == 12
        gd_error("gd-jpeg: error: jpeg library was compiled for 12-bit\n"
                 "precision. This is mostly useless, because JPEGs on the web are\n"
                 "8-bit and such versions of the jpeg library won't read or write\n"
                 "them. GD doesn't support these unusual images. Edit your\n"
                 "jmorecfg.h file to specify the correct precision and completely\n"
                 "'make clean' and 'make install' libjpeg again. Sorry.\n");
        goto error;
#endif /* BITS_IN_JSAMPLE == 12 */
        for (i = 0; i < im->sy; i++) {
            for (jidx = 0, j = 0; j < im->sx; j++) {
                int val = im->tpixels[i][j];
                row[jidx++] = gdTrueColorGetRed(val);
                row[jidx++] = gdTrueColorGetGreen(val);
                row[jidx++] = gdTrueColorGetBlue(val);
            }

            nlines = jpeg_write_scanlines(&cinfo, rowptr, 1);

            if (nlines != 1) {
                gd_error("gd_jpeg: warning: jpeg_write_scanlines returns %u -- "
                         "expected 1\n",
                         nlines);
            }
        }
    } else {
        for (i = 0; i < im->sy; i++) {
            for (jidx = 0, j = 0; j < im->sx; j++) {
                int idx = im->pixels[i][j];

                /*
                 * NB: Although gd RGB values are ints, their max value is
                 * 255 (see the documentation for gdImageColorAllocate())
                 * -- perfect for 8-bit JPEG encoding (which is the norm)
                 */
#if BITS_IN_JSAMPLE == 8
                row[jidx++] = im->red[idx];
                row[jidx++] = im->green[idx];
                row[jidx++] = im->blue[idx];
#elif BITS_IN_JSAMPLE == 12
                row[jidx++] = im->red[idx] << 4;
                row[jidx++] = im->green[idx] << 4;
                row[jidx++] = im->blue[idx] << 4;
#else
#error IJG JPEG library BITS_IN_JSAMPLE value must be 8 or 12
#endif
            }

            nlines = jpeg_write_scanlines(&cinfo, rowptr, 1);
            if (nlines != 1) {
                gd_error("gd_jpeg: warning: jpeg_write_scanlines"
                         " returns %u -- expected 1\n",
                         nlines);
            }
        }
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    gdFree(row);
    return 0;

error:
    jpeg_destroy_compress(&cinfo);
    if (row) {
        gdFree(row);
    }
    return 1;
}

BGD_DECLARE(gdImagePtr) gdImageCreateFromJpeg(FILE *inFile)
{
    return gdImageCreateFromJpegEx(inFile, 1);
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegEx(FILE *inFile, int ignore_warning)
{
    gdImagePtr im;
    gdJpegReadOptions options;
    gdIOCtx *in = gdNewFileCtx(inFile);
    if (in == NULL)
        return NULL;
    gdJpegReadOptionsInit(&options);
    options.ignore_warning = ignore_warning;
    im = gdImageCreateFromJpegCtxWithOptions(in, &options);
    in->gd_free(in);
    return im;
}

BGD_DECLARE(gdImagePtr) gdImageCreateFromJpegPtr(int size, void *data)
{
    return gdImageCreateFromJpegPtrEx(size, data, 1);
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrEx(int size, void *data, int ignore_warning)
{
    gdImagePtr im;
    gdJpegReadOptions options;
    gdIOCtx *in = gdNewDynamicCtxEx(size, data, 0);
    if (!in) {
        return 0;
    }
    gdJpegReadOptionsInit(&options);
    options.ignore_warning = ignore_warning;
    im = gdImageCreateFromJpegCtxWithOptions(in, &options);
    in->gd_free(in);
    return im;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrWithOptions(int size, void *data, const gdJpegReadOptions *options)
{
    gdImagePtr im;
    gdIOCtx *in = gdNewDynamicCtxEx(size, data, 0);
    if (!in) {
        return 0;
    }
    im = gdImageCreateFromJpegCtxWithOptions(in, options);
    in->gd_free(in);
    return im;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrWithMetadata(int size, void *data, gdImageMetadata *metadata)
{
    return gdImageCreateFromJpegPtrExWithMetadata(size, data, 1, metadata);
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrExWithMetadata(int size, void *data, int ignore_warning,
                                       gdImageMetadata *metadata)
{
    gdImagePtr im;
    gdIOCtx *in = gdNewDynamicCtxEx(size, data, 0);
    if (!in) {
        return 0;
    }
    im = gdImageCreateFromJpegCtxExWithMetadata(in, ignore_warning, metadata);
    in->gd_free(in);
    return im;
}

static void jpeg_gdIOCtx_src(j_decompress_ptr cinfo, gdIOCtx *infile);

static int CMYKToRGB(int c, int m, int y, int k, int inverted);

static gdImagePtr gdImageCreateFromJpegCtxWithOptionsAndMetadata(
    gdIOCtx *infile, const gdJpegReadOptions *options, gdImageMetadata *metadata);

static unsigned int gdJpegGcd(unsigned int a, unsigned int b)
{
    while (b != 0) {
        unsigned int t = b;
        b = a % b;
        a = t;
    }
    return a == 0 ? 1 : a;
}

static int gdJpegNormalizeReadOptions(const gdJpegReadOptions *options,
                                      gdJpegReadOptions *normalized)
{
    unsigned int divisor;

    gdJpegReadOptionsInit(normalized);
    if (options != NULL) {
        normalized->ignore_warning = options->ignore_warning;
        normalized->scale_num = options->scale_num;
        normalized->scale_denom = options->scale_denom;
        normalized->dct_method = options->dct_method;
    }

    if (normalized->scale_num == 0 || normalized->scale_denom == 0) {
        return 0;
    }

    divisor = gdJpegGcd(normalized->scale_num, normalized->scale_denom);
    normalized->scale_num /= divisor;
    normalized->scale_denom /= divisor;

    switch (normalized->dct_method) {
    case GD_JPEG_DCT_DEFAULT:
    case GD_JPEG_DCT_SLOW:
    case GD_JPEG_DCT_FAST:
    case GD_JPEG_DCT_FLOAT:
        break;
    default:
        return 0;
    }

    return 1;
}

static J_DCT_METHOD gdJpegMapDctMethod(int dct_method)
{
    switch (dct_method) {
    case GD_JPEG_DCT_SLOW:
        return JDCT_ISLOW;
    case GD_JPEG_DCT_FAST:
        return JDCT_IFAST;
    case GD_JPEG_DCT_FLOAT:
        return JDCT_FLOAT;
    case GD_JPEG_DCT_DEFAULT:
    default:
        return JDCT_DEFAULT;
    }
}

static int gdJpegScaledDimension(JDIMENSION source, unsigned int numerator,
                                 unsigned int denominator, int *target)
{
    unsigned long long scaled;

    scaled = ((unsigned long long)source * (unsigned long long)numerator) /
             (unsigned long long)denominator;
    if (scaled == 0 || scaled > INT_MAX) {
        return 0;
    }

    *target = (int)scaled;
    return 1;
}

static int gdJpegMarkerStartsWith(jpeg_saved_marker_ptr marker, const unsigned char *prefix,
                                  size_t prefix_size)
{
    return marker->data_length >= prefix_size && memcmp(marker->data, prefix, prefix_size) == 0;
}

static int gdJpegCollectIccProfile(j_decompress_ptr cinfo, gdImageMetadata *metadata)
{
    static const unsigned char icc_signature[] = "ICC_PROFILE";
    jpeg_saved_marker_ptr marker;
    jpeg_saved_marker_ptr segments[256];
    unsigned int segment_sizes[256];
    unsigned int segment_count = 0;
    unsigned int i;
    size_t total_size = 0;
    size_t offset = 0;
    unsigned char *icc;
    int status;

    memset(segments, 0, sizeof(segments));
    memset(segment_sizes, 0, sizeof(segment_sizes));

    for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
        unsigned int sequence;
        unsigned int count;

        if (marker->marker != JPEG_APP0 + 2 || !gdJpegMarkerStartsWith(marker, icc_signature, 12) ||
            marker->data_length < 14) {
            continue;
        }

        sequence = marker->data[12];
        count = marker->data[13];
        if (sequence == 0 || count == 0 || sequence > count) {
            return GD_META_ERR_PARSE;
        }
        if (segment_count == 0) {
            segment_count = count;
        } else if (segment_count != count) {
            return GD_META_ERR_PARSE;
        }
        if (segments[sequence] != NULL) {
            return GD_META_ERR_PARSE;
        }
        segments[sequence] = marker;
        segment_sizes[sequence] = marker->data_length - 14;
        if ((size_t)-1 - total_size < segment_sizes[sequence]) {
            return GD_META_ERR_LIMIT;
        }
        total_size += segment_sizes[sequence];
    }

    if (segment_count == 0) {
        return GD_META_OK;
    }

    for (i = 1; i <= segment_count; i++) {
        if (segments[i] == NULL) {
            return GD_META_ERR_PARSE;
        }
    }

    icc = (unsigned char *)gdMalloc(total_size);
    if (icc == NULL && total_size != 0) {
        return GD_META_ERR_NOMEM;
    }

    for (i = 1; i <= segment_count; i++) {
        if (segment_sizes[i] != 0) {
            // codechecker_false_positive [all] suppress all checker results
            memcpy(icc + offset, segments[i]->data + 14, segment_sizes[i]);
        }
        offset += segment_sizes[i];
    }

    status = gdImageMetadataSetProfile(metadata, "icc", icc, total_size);
    if (icc != NULL) {
        gdFree(icc);
    }
    return status;
}

static int gdJpegCollectMetadata(j_decompress_ptr cinfo, gdImageMetadata *metadata)
{
    static const unsigned char exif_signature[] = {'E', 'x', 'i', 'f', '\0', '\0'};
    static const unsigned char xmp_signature[] = "http://ns.adobe.com/xap/1.0/";
    static const unsigned char iptc_signature[] = "Photoshop 3.0";
    jpeg_saved_marker_ptr marker;
    int status;

    if (metadata == NULL) {
        return GD_META_OK;
    }

    for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
        if (marker->marker == JPEG_APP0 + 1 &&
            gdJpegMarkerStartsWith(marker, exif_signature, sizeof(exif_signature))) {
            status = gdImageMetadataSetProfile(metadata, "exif", marker->data, marker->data_length);
            if (status != GD_META_OK) {
                return status;
            }
        } else if (marker->marker == JPEG_APP0 + 1 &&
                   gdJpegMarkerStartsWith(marker, xmp_signature, sizeof(xmp_signature))) {
            status = gdImageMetadataSetProfile(metadata, "xmp", marker->data, marker->data_length);
            if (status != GD_META_OK) {
                return status;
            }
        } else if (marker->marker == JPEG_APP0 + 13 &&
                   gdJpegMarkerStartsWith(marker, iptc_signature, sizeof(iptc_signature))) {
            status = gdImageMetadataSetProfile(metadata, "iptc", marker->data, marker->data_length);
            if (status != GD_META_OK) {
                return status;
            }
        }
    }

    return gdJpegCollectIccProfile(cinfo, metadata);
}

BGD_DECLARE(gdImagePtr) gdImageCreateFromJpegCtx(gdIOCtx *infile)
{
    return gdImageCreateFromJpegCtxEx(infile, 1);
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxWithMetadata(gdIOCtx *infile, gdImageMetadata *metadata)
{
    return gdImageCreateFromJpegCtxExWithMetadata(infile, 1, metadata);
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxEx(gdIOCtx *infile, int ignore_warning)
{
    return gdImageCreateFromJpegCtxExWithMetadata(infile, ignore_warning, NULL);
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxExWithMetadata(gdIOCtx *infile, int ignore_warning,
                                       gdImageMetadata *metadata)
{
    gdJpegReadOptions options;

    gdJpegReadOptionsInit(&options);
    options.ignore_warning = ignore_warning;
    return gdImageCreateFromJpegCtxWithOptionsAndMetadata(infile, &options, metadata);
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxWithOptions(gdIOCtx *infile, const gdJpegReadOptions *options)
{
    return gdImageCreateFromJpegCtxWithOptionsAndMetadata(infile, options, NULL);
}

static gdImagePtr gdImageCreateFromJpegCtxWithOptionsAndMetadata(
    gdIOCtx *infile, const gdJpegReadOptions *options, gdImageMetadata *metadata)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jmpbuf_wrapper jmpbufw;
    gdJpegReadOptions read_options;
    /* volatile so we can gdFree them after longjmp */
    volatile JSAMPROW row = 0;
    volatile gdImagePtr im = 0;
    JSAMPROW rowptr[1];
    JDIMENSION i, j;
    int retval;
    JDIMENSION nrows;
    int channels = 3;
    int inverted = 0;
    int target_width = 0;
    int target_height = 0;

#ifdef JPEG_DEBUG
    gd_error_ex(GD_DEBUG, "gd-jpeg: gd JPEG version %s\n", GD_JPEG_VERSION);
    gd_error_ex(GD_DEBUG, "gd-jpeg: JPEG library version %d, %d-bit sample values\n",
                JPEG_LIB_VERSION, BITS_IN_JSAMPLE);
    gd_error_ex(GD_DEBUG, "sizeof: %d\n", sizeof(struct jpeg_decompress_struct));
#endif

    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));

    if (!gdJpegNormalizeReadOptions(options, &read_options)) {
        gd_error("gd-jpeg: error: invalid JPEG read options\n");
        return 0;
    }

    jmpbufw.ignore_warning = read_options.ignore_warning;

    cinfo.err = jpeg_std_error(&jerr);
    cinfo.client_data = &jmpbufw;

    cinfo.err->emit_message = jpeg_emit_message;

    if (setjmp(jmpbufw.jmpbuf) != 0) {
        /* we're here courtesy of longjmp */
        if (row) {
            gdFree(row);
        }
        if (im) {
            gdImageDestroy(im);
        }
        return 0;
    }

    cinfo.err->error_exit = fatal_jpeg_error;

    jpeg_create_decompress(&cinfo);

    jpeg_gdIOCtx_src(&cinfo, infile);

    /* 2.0.22: save the APP14 marker to check for Adobe Photoshop CMYK
     * files with inverted components.
     */
    jpeg_save_markers(&cinfo, JPEG_APP0 + 14, 256);
    if (metadata != NULL) {
        jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF);
        jpeg_save_markers(&cinfo, JPEG_APP0 + 2, 0xFFFF);
        jpeg_save_markers(&cinfo, JPEG_APP0 + 13, 0xFFFF);
    }

    retval = jpeg_read_header(&cinfo, TRUE);
    if (retval != JPEG_HEADER_OK) {
        gd_error("gd-jpeg: warning: jpeg_read_header returns"
                 " %d, expected %d\n",
                 retval, JPEG_HEADER_OK);
    }

    retval = gdJpegCollectMetadata(&cinfo, metadata);
    if (retval != GD_META_OK) {
        gd_error("gd-jpeg: error: unable to read metadata\n");
        goto error;
    }

    if (cinfo.image_height > INT_MAX) {
        gd_error("gd-jpeg: warning: JPEG image height (%u) is"
                 " greater than INT_MAX (%d) (and thus greater than"
                 " gd can handle)",
                 cinfo.image_height, INT_MAX);
    }

    if (cinfo.image_width > INT_MAX) {
        gd_error("gd-jpeg: warning: JPEG image width (%u) is"
                 " greater than INT_MAX (%d) (and thus greater than"
                 " gd can handle)\n",
                 cinfo.image_width, INT_MAX);
    }

    if (!gdJpegScaledDimension(cinfo.image_width, read_options.scale_num,
                               read_options.scale_denom, &target_width) ||
        !gdJpegScaledDimension(cinfo.image_height, read_options.scale_num,
                               read_options.scale_denom, &target_height)) {
        gd_error("gd-jpeg: error: requested JPEG scale produces invalid dimensions\n");
        goto error;
    }

    /* 2.0.22: very basic support for reading CMYK colorspace files. Nice for
     * thumbnails but there's no support for fussy adjustment of the
     * assumed properties of inks and paper.
     */
    if ((cinfo.jpeg_color_space == JCS_CMYK) || (cinfo.jpeg_color_space == JCS_YCCK)) {
        cinfo.out_color_space = JCS_CMYK;
    } else {
        cinfo.out_color_space = JCS_RGB;
    }

    cinfo.scale_num = read_options.scale_num;
    cinfo.scale_denom = read_options.scale_denom;
    cinfo.dct_method = gdJpegMapDctMethod(read_options.dct_method);

    if (jpeg_start_decompress(&cinfo) != TRUE) {
        gd_error("gd-jpeg: warning: jpeg_start_decompress"
                 " reports suspended data source\n");
    }

    if (cinfo.output_width > INT_MAX || cinfo.output_height > INT_MAX) {
        gd_error("gd-jpeg: error: JPEG output dimensions are greater than INT_MAX\n");
        goto error;
    }

    im = gdImageCreateTrueColor((int)cinfo.output_width, (int)cinfo.output_height);
    if (im == 0) {
        gd_error("gd-jpeg error: cannot allocate gdImage struct\n");
        goto error;
    }

    /* check if the resolution is specified */
    switch (cinfo.density_unit) {
    case 1:
        im->res_x = cinfo.X_density;
        im->res_y = cinfo.Y_density;
        break;
    case 2:
        im->res_x = DPCM2DPI(cinfo.X_density);
        im->res_y = DPCM2DPI(cinfo.Y_density);
        break;
    }

#ifdef JPEG_DEBUG
    gd_error_ex(GD_DEBUG, "gd-jpeg: JPEG image information:");
    if (cinfo.saw_JFIF_marker) {
        gd_error_ex(GD_DEBUG, " JFIF version %d.%.2d", (int)cinfo.JFIF_major_version,
                    (int)cinfo.JFIF_minor_version);
    } else if (cinfo.saw_Adobe_marker) {
        gd_error_ex(GD_DEBUG, " Adobe format");
    } else {
        gd_error_ex(GD_DEBUG, " UNKNOWN format");
    }

    gd_error_ex(GD_DEBUG, " %ux%u (raw) / %ux%u (scaled) %d-bit", cinfo.image_width,
                cinfo.image_height, cinfo.output_width, cinfo.output_height, cinfo.data_precision);
    gd_error_ex(GD_DEBUG, " %s", (cinfo.progressive_mode ? "progressive" : "baseline"));
    gd_error_ex(GD_DEBUG, " image, %d quantized colors, ", cinfo.actual_number_of_colors);

    switch (cinfo.jpeg_color_space) {
    case JCS_GRAYSCALE:
        gd_error_ex(GD_DEBUG, "grayscale");
        break;

    case JCS_RGB:
        gd_error_ex(GD_DEBUG, "RGB");
        break;

    case JCS_YCbCr:
        gd_error_ex(GD_DEBUG, "YCbCr (a.k.a. YUV)");
        break;

    case JCS_CMYK:
        gd_error_ex(GD_DEBUG, "CMYK");
        break;

    case JCS_YCCK:
        gd_error_ex(GD_DEBUG, "YCbCrK");
        break;

    default:
        gd_error_ex(GD_DEBUG, "UNKNOWN (value: %d)", (int)cinfo.jpeg_color_space);
        break;
    }

    gd_error_ex(GD_DEBUG, " colorspace\n");
    fflush(stdout);
#endif /* JPEG_DEBUG */

    /* REMOVED by TBB 2/12/01. This field of the structure is
     * documented as private, and sure enough it's gone in the
     * latest libjpeg, replaced by something else. Unfortunately
     * there is still no right way to find out if the file was
     * progressive or not; just declare your intent before you
     * write one by calling gdImageInterlace(im, 1) yourself.
     * After all, we're not really supposed to rework JPEGs and
     * write them out again anyway. Lossy compression, remember? */
#if 0
	gdImageInterlace (im, cinfo.progressive_mode != 0);
#endif
    if (cinfo.out_color_space == JCS_RGB) {
        if (cinfo.output_components != 3) {
            gd_error("gd-jpeg: error: JPEG color quantization"
                     " request resulted in output_components == %d"
                     " (expected 3 for RGB)\n",
                     cinfo.output_components);
            goto error;
        }
        channels = 3;
    } else if (cinfo.out_color_space == JCS_CMYK) {
        jpeg_saved_marker_ptr marker;
        if (cinfo.output_components != 4) {
            gd_error("gd-jpeg: error: JPEG color quantization"
                     " request resulted in output_components == %d"
                     " (expected 4 for CMYK)\n",
                     cinfo.output_components);
            goto error;
        }
        channels = 4;

        marker = cinfo.marker_list;
        while (marker) {
            if ((marker->marker == (JPEG_APP0 + 14)) && (marker->data_length >= 12) &&
                (!strncmp((const char *)marker->data, "Adobe", 5))) {
                inverted = 1;
                break;
            }
            marker = marker->next;
        }
    } else {
        gd_error("gd-jpeg: error: unexpected colorspace\n");
        goto error;
    }
#if BITS_IN_JSAMPLE == 12
    gd_error_ex(GD_ERROR, "gd-jpeg: error: jpeg library was compiled for 12-bit\n"
                          "precision. This is mostly useless, because JPEGs on the web are\n"
                          "8-bit and such versions of the jpeg library won't read or write\n"
                          "them. GD doesn't support these unusual images. Edit your\n"
                          "jmorecfg.h file to specify the correct precision and completely\n"
                          "'make clean' and 'make install' libjpeg again. Sorry.\n");
    goto error;
#endif /* BITS_IN_JSAMPLE == 12 */

    row = gdCalloc(cinfo.output_width * channels, sizeof(JSAMPLE));
    if (row == 0) {
        gd_error("gd-jpeg: error: unable to allocate row for"
                 " JPEG scanline: gdCalloc returns NULL\n");
        goto error;
    }
    rowptr[0] = row;
    if (cinfo.out_color_space == JCS_CMYK) {
        for (i = 0; i < cinfo.output_height; i++) {
            register JSAMPROW currow = row;
            register int *tpix = im->tpixels[i];
            nrows = jpeg_read_scanlines(&cinfo, rowptr, 1);
            if (nrows != 1) {
                gd_error("gd-jpeg: error: jpeg_read_scanlines"
                         " returns %u, expected 1\n",
                         nrows);
                goto error;
            }
            for (j = 0; j < cinfo.output_width; j++, currow += 4, tpix++) {
                *tpix = CMYKToRGB(currow[0], currow[1], currow[2], currow[3], inverted);
            }
        }
    } else {
        for (i = 0; i < cinfo.output_height; i++) {
            register JSAMPROW currow = row;
            register int *tpix = im->tpixels[i];
            nrows = jpeg_read_scanlines(&cinfo, rowptr, 1);
            if (nrows != 1) {
                gd_error("gd-jpeg: error: jpeg_read_scanlines"
                         " returns %u, expected 1\n",
                         nrows);
                goto error;
            }
            for (j = 0; j < cinfo.output_width; j++, currow += 3, tpix++) {
                *tpix = gdTrueColor(currow[0], currow[1], currow[2]);
            }
        }
    }

    if (jpeg_finish_decompress(&cinfo) != TRUE) {
        gd_error("gd-jpeg: warning: jpeg_finish_decompress"
                 " reports suspended data source\n");
    }
    /* TBB 2.0.29: we should do our best to read whatever we can read, and a
     * warning is a warning. A fatal error on warnings doesn't make sense. */
#if 0
	/* This was originally added by Truxton Fulton */
	if (cinfo.err->num_warnings > 0)
		goto error;
#endif

    jpeg_destroy_decompress(&cinfo);
    gdFree(row);

    if (im->sx != target_width || im->sy != target_height) {
        gdImagePtr scaled = gdImageScale(im, target_width, target_height);
        gdImageDestroy(im);
        im = scaled;
        if (im == NULL) {
            gd_error("gd-jpeg: error: unable to resize decoded JPEG image\n");
            return 0;
        }
    }

    return im;

error:
    jpeg_destroy_decompress(&cinfo);

    if (row) {
        gdFree(row);
    }
    if (im) {
        gdImageDestroy(im);
    }

    return 0;
}

/* A very basic conversion approach, TBB */

static int CMYKToRGB(int c, int m, int y, int k, int inverted)
{
    if (inverted) {
        c = 255 - c;
        m = 255 - m;
        y = 255 - y;
        k = 255 - k;
    }

    return gdTrueColor((255 - c) * (255 - k) / 255, (255 - m) * (255 - k) / 255,
                       (255 - y) * (255 - k) / 255);
#if 0
	if (inverted) {
		c = 255 - c;
		m = 255 - m;
		y = 255 - y;
		k = 255 - k;
	}
	c = c * (255 - k) / 255 + k;
	if (c > 255) {
		c = 255;
	}
	if (c < 0) {
		c = 0;
	}
	m = m * (255 - k) / 255 + k;
	if (m > 255) {
		m = 255;
	}
	if (m < 0) {
		m = 0;
	}
	y = y * (255 - k) / 255 + k;
	if (y > 255) {
		y = 255;
	}
	if (y < 0) {
		y = 0;
	}
	c = 255 - c;
	m = 255 - m;
	y = 255 - y;
	return gdTrueColor (c, m, y);
#endif
}

/*
 * gdIOCtx JPEG data sources and sinks, T. Boutell
 * almost a simple global replace from T. Lane's stdio versions.
 */

/* Expanded data source object for gdIOCtx input */
typedef struct {
    struct jpeg_source_mgr pub; /* public fields */
    gdIOCtx *infile;            /* source stream */
    unsigned char *buffer;      /* start of buffer */
    boolean start_of_file;      /* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr *my_src_ptr;

#define INPUT_BUF_SIZE 4096 /* choose an efficiently fread'able size */

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

static void init_source(j_decompress_ptr cinfo)
{
    my_src_ptr src = (my_src_ptr)cinfo->src;

    /* We reset the empty-input-file flag for each image,
     * but we don't clear the input buffer.
     * This is correct behavior for reading a series of images from one source.
     */
    src->start_of_file = TRUE;
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * In typical applications, this should read fresh data into the buffer
 * (ignoring the current state of next_input_byte & bytes_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been reloaded.  It is not necessary to
 * fill the buffer entirely, only to obtain at least one more byte.
 *
 * There is no such thing as an EOF return.  If the end of the file has been
 * reached, the routine has a choice of ERREXIT() or inserting fake data into
 * the buffer.  In most cases, generating a warning message and inserting a
 * fake EOI marker is the best course of action --- this will allow the
 * decompressor to output however much of the image is there.  However,
 * the resulting error message is misleading if the real problem is an empty
 * input file, so we handle that case specially.
 *
 * In applications that need to be able to suspend compression due to input
 * not being available yet, a FALSE return indicates that no more data can be
 * obtained right now, but more may be forthcoming later.  In this situation,
 * the decompressor will return to its caller (with an indication of the
 * number of scanlines it has read, if any).  The application should resume
 * decompression after it has loaded more data into the input buffer.  Note
 * that there are substantial restrictions on the use of suspension --- see
 * the documentation.
 *
 * When suspending, the decompressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_input_byte & bytes_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point must be rescanned after resumption, so move it to
 * the front of the buffer rather than discarding it.
 */

#define END_JPEG_SEQUENCE "\r\n[*]--:END JPEG:--[*]\r\n"

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
    my_src_ptr src = (my_src_ptr)cinfo->src;
    /* 2.0.12: signed size. Thanks to Geert Jansen */
    ssize_t nbytes = 0;
    memset(src->buffer, 0, INPUT_BUF_SIZE);

    while (nbytes < INPUT_BUF_SIZE) {
        int got = gdGetBuf(src->buffer + nbytes, INPUT_BUF_SIZE - nbytes, src->infile);

        if ((got == EOF) || (got == 0)) {
            /* EOF or error. If we got any data, don't worry about it.
             * If we didn't, then this is unexpected. */
            if (!nbytes) {
                nbytes = -1;
            }
            break;
        }
        nbytes += got;
    }

    if (nbytes <= 0) {
        if (src->start_of_file) {
            /* Treat empty input file as fatal error */
            ERREXIT(cinfo, JERR_INPUT_EMPTY);
        }
        WARNMS(cinfo, JWRN_JPEG_EOF);
        /* Insert a fake EOI marker */
        src->buffer[0] = (unsigned char)0xFF;
        src->buffer[1] = (unsigned char)JPEG_EOI;
        nbytes = 2;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;
    src->start_of_file = FALSE;

    return TRUE;
}

/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 */

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    my_src_ptr src = (my_src_ptr)cinfo->src;

    /* Just a dumb implementation for now. Not clear that being smart is worth
     * any trouble anyway --- large skips are infrequent.
     */
    if (num_bytes > 0) {
        while (num_bytes > (long)src->pub.bytes_in_buffer) {
            num_bytes -= (long)src->pub.bytes_in_buffer;
            (void)fill_input_buffer(cinfo);
            /* note we assume that fill_input_buffer will never return FALSE,
             * so suspension need not be handled.
             */
        }
        src->pub.next_input_byte += (size_t)num_bytes;
        src->pub.bytes_in_buffer -= (size_t)num_bytes;
    }
}

/*
 * An additional method that can be provided by data source modules is the
 * resync_to_restart method for error recovery in the presence of RST markers.
 * For the moment, this source module just uses the default resync method
 * provided by the JPEG library.  That method assumes that no backtracking
 * is possible.
 */

/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */
static void term_source(j_decompress_ptr cinfo) { (void)cinfo; }

/*
 * Prepare for input from a gdIOCtx stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing decompression.
 */

static void jpeg_gdIOCtx_src(j_decompress_ptr cinfo, gdIOCtx *infile)
{
    my_src_ptr src;

    /* The source object and input buffer are made permanent so that a series
     * of JPEG images can be read from the same file by calling jpeg_gdIOCtx_src
     * only before the first one.  (If we discarded the buffer at the end of
     * one image, we'd likely lose the start of the next one.)
     * This makes it unsafe to use this manager and a different source
     * manager serially with the same JPEG object.  Caveat programmer.
     */
    if (cinfo->src == NULL) {
        /* first time for this JPEG object? */
        cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));
        src = (my_src_ptr)cinfo->src;
        src->buffer = (unsigned char *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT, INPUT_BUF_SIZE * sizeof(unsigned char));
    }

    src = (my_src_ptr)cinfo->src;
    src->pub.init_source = init_source;
    src->pub.fill_input_buffer = fill_input_buffer;
    src->pub.skip_input_data = skip_input_data;
    src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
    src->pub.term_source = term_source;
    src->infile = infile;
    src->pub.bytes_in_buffer = 0;    /* forces fill_input_buffer on first read */
    src->pub.next_input_byte = NULL; /* until buffer loaded */
}

/* Expanded data destination object for stdio output */
typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */
    gdIOCtx *outfile;                /* target stream */
    unsigned char *buffer;           /* start of buffer */
} my_destination_mgr;

typedef my_destination_mgr *my_dest_ptr;

#define OUTPUT_BUF_SIZE 4096 /* choose an efficiently fwrite'able size */

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

static void init_destination(j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image
     */
    dest->buffer = (unsigned char *)(*cinfo->mem->alloc_small)(
        (j_common_ptr)cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(unsigned char));

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

static boolean empty_output_buffer(j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;

    if (gdPutBuf(dest->buffer, OUTPUT_BUF_SIZE, dest->outfile) != (size_t)OUTPUT_BUF_SIZE) {
        ERREXIT(cinfo, JERR_FILE_WRITE);
    }

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

static void term_destination(j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    if (datacount > 0) {
        if ((size_t)gdPutBuf(dest->buffer, datacount, dest->outfile) != datacount) {
            ERREXIT(cinfo, JERR_FILE_WRITE);
        }
    }
}

/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

static void jpeg_gdIOCtx_dest(j_compress_ptr cinfo, gdIOCtx *outfile)
{
    my_dest_ptr dest;

    /* The destination object is made permanent so that multiple JPEG images
     * can be written to the same file without re-executing jpeg_stdio_dest.
     * This makes it dangerous to use this manager and a different destination
     * manager serially with the same JPEG object, because their private object
     * sizes may be different.  Caveat programmer.
     */
    if (cinfo->dest == NULL) {
        /* first time for this JPEG object? */
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(my_destination_mgr));
    }

    dest = (my_dest_ptr)cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outfile = outfile;
}

#else /* !HAVE_LIBJPEG */

static void _noJpegError(void) { gd_error("JPEG image support has been disabled\n"); }

BGD_DECLARE(void) gdJpegInfoInit(gdJpegInfo *info)
{
    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
}

BGD_DECLARE(void) gdJpegReadOptionsInit(gdJpegReadOptions *options)
{
    if (options != NULL) {
        memset(options, 0, sizeof(*options));
        options->ignore_warning = 1;
        options->scale_num = 1;
        options->scale_denom = 1;
        options->dct_method = GD_JPEG_DCT_DEFAULT;
    }
}

BGD_DECLARE(void) gdJpegWriteOptionsInit(gdJpegWriteOptions *options)
{
    if (options != NULL) {
        memset(options, 0, sizeof(*options));
        options->quality = -1;
    }
}

BGD_DECLARE(int) gdJpegGetInfo(FILE *infile, gdJpegInfo *info)
{
    ARG_NOT_USED(infile);
    ARG_NOT_USED(info);
    _noJpegError();
    return 1;
}

BGD_DECLARE(int) gdJpegGetInfoCtx(gdIOCtxPtr infile, gdJpegInfo *info)
{
    ARG_NOT_USED(infile);
    ARG_NOT_USED(info);
    _noJpegError();
    return 1;
}

BGD_DECLARE(int) gdJpegGetInfoPtr(int size, const void *data, gdJpegInfo *info)
{
    ARG_NOT_USED(size);
    ARG_NOT_USED(data);
    ARG_NOT_USED(info);
    _noJpegError();
    return 1;
}

BGD_DECLARE(void) gdImageJpeg(gdImagePtr im, FILE *outFile, int quality)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(outFile);
    ARG_NOT_USED(quality);
    _noJpegError();
}

BGD_DECLARE(void *) gdImageJpegPtr(gdImagePtr im, int *size, int quality)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(size);
    ARG_NOT_USED(quality);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(void *)
gdImageJpegPtrWithMetadata(gdImagePtr im, int *size, int quality, const gdImageMetadata *metadata)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(size);
    ARG_NOT_USED(quality);
    ARG_NOT_USED(metadata);
    _noJpegError();
    return NULL;
}

void *gdImageJpegPtrWithMetadataNoSubsampling(gdImagePtr im, int *size, int quality,
                                              const gdImageMetadata *metadata)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(size);
    ARG_NOT_USED(quality);
    ARG_NOT_USED(metadata);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(void) gdImageJpegCtx(gdImagePtr im, gdIOCtx *outfile, int quality)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(outfile);
    ARG_NOT_USED(quality);
    _noJpegError();
}

BGD_DECLARE(void)
gdImageJpegCtxWithMetadata(gdImagePtr im, gdIOCtx *outfile, int quality,
                           const gdImageMetadata *metadata)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(outfile);
    ARG_NOT_USED(quality);
    ARG_NOT_USED(metadata);
    _noJpegError();
}

BGD_DECLARE(int)
gdImageJpegWithOptions(gdImagePtr im, FILE *outFile, const gdJpegWriteOptions *options)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(outFile);
    ARG_NOT_USED(options);
    _noJpegError();
    return 1;
}

BGD_DECLARE(int)
gdImageJpegCtxWithOptions(gdImagePtr im, gdIOCtx *outfile, const gdJpegWriteOptions *options)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(outfile);
    ARG_NOT_USED(options);
    _noJpegError();
    return 1;
}

BGD_DECLARE(void *)
gdImageJpegPtrWithOptions(gdImagePtr im, int *size, const gdJpegWriteOptions *options)
{
    ARG_NOT_USED(im);
    ARG_NOT_USED(size);
    ARG_NOT_USED(options);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr) gdImageCreateFromJpeg(FILE *inFile)
{
    ARG_NOT_USED(inFile);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegEx(FILE *inFile, int ignore_warning)
{
    ARG_NOT_USED(inFile);
    ARG_NOT_USED(ignore_warning);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr) gdImageCreateFromJpegPtr(int size, void *data)
{
    ARG_NOT_USED(size);
    ARG_NOT_USED(data);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrEx(int size, void *data, int ignore_warning)
{
    ARG_NOT_USED(size);
    ARG_NOT_USED(data);
    ARG_NOT_USED(ignore_warning);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrWithOptions(int size, void *data, const gdJpegReadOptions *options)
{
    ARG_NOT_USED(size);
    ARG_NOT_USED(data);
    ARG_NOT_USED(options);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrWithMetadata(int size, void *data, gdImageMetadata *metadata)
{
    ARG_NOT_USED(size);
    ARG_NOT_USED(data);
    ARG_NOT_USED(metadata);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegPtrExWithMetadata(int size, void *data, int ignore_warning,
                                       gdImageMetadata *metadata)
{
    ARG_NOT_USED(size);
    ARG_NOT_USED(data);
    ARG_NOT_USED(ignore_warning);
    ARG_NOT_USED(metadata);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr) gdImageCreateFromJpegCtx(gdIOCtx *infile)
{
    ARG_NOT_USED(infile);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxEx(gdIOCtx *infile, int ignore_warning)
{
    ARG_NOT_USED(infile);
    ARG_NOT_USED(ignore_warning);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxWithOptions(gdIOCtx *infile, const gdJpegReadOptions *options)
{
    ARG_NOT_USED(infile);
    ARG_NOT_USED(options);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxWithMetadata(gdIOCtx *infile, gdImageMetadata *metadata)
{
    ARG_NOT_USED(infile);
    ARG_NOT_USED(metadata);
    _noJpegError();
    return NULL;
}

BGD_DECLARE(gdImagePtr)
gdImageCreateFromJpegCtxExWithMetadata(gdIOCtx *infile, int ignore_warning,
                                       gdImageMetadata *metadata)
{
    ARG_NOT_USED(infile);
    ARG_NOT_USED(ignore_warning);
    ARG_NOT_USED(metadata);
    _noJpegError();
    return NULL;
}

#endif /* HAVE_LIBJPEG */
