# libgd 2.4 — Codec Format Support

Summary of every image codec available in libgd 2.4: supported formats, how
truecolor and palette images are handled on input and output, option structs,
named constants, and any metadata or animation API surfaces.

---

## At-a-glance

| Codec   | Read               | Write                  | Animation              |
|---------|--------------------|------------------------|------------------------|
| JPEG    | Yes                | Yes                    | No                     |
| PNG     | Yes                | Yes                    | No                     |
| GIF     | Yes                | Yes                    | Yes (read/write)       |
| BMP     | Yes                | Yes                    | No                     |
| WebP    | Yes                | Yes                    | Yes (read/write)       |
| TIFF    | Yes                | Yes                    | Yes (multi-page read + multi-page write) |
| JXL     | Yes                | Yes                    | Yes (read/write)       |
| AVIF    | Yes                | Yes                    | No (reads first frame) |
| HEIF    | Yes                | Yes                    | No                     |
| QOI     | Yes                | Yes                    | No                     |
| TGA     | Yes                | No                     | No                     |
| WBMP    | Yes                | Yes                    | No                     |
| XBM     | Yes                | Yes                    | No                     |
| XPM     | Yes                | No                     | No                     |

All codecs are optional at compile time (`HAVE_LIBJPEG`, `HAVE_LIBPNG`,
`HAVE_LIBWEBP`, `HAVE_LIBAVIF`, `HAVE_LIBHEIF`, `HAVE_LIBJXL`,
`HAVE_LIBTIFF`, `HAVE_LIBXPM`, `HAVE_LIBGD_GIF`, etc.).  The stubs return
NULL / no-op when a codec is disabled.

---

## JPEG (`src/gd_jpeg.c`)

**Read:** only 8-bit JSAMPLE mode is supported (12-bit IJG builds are
rejected).  Always returns a **truecolor** image (`gdImageCreateTrueColor`).

| JPEG color space       | Output to gd                          |
|------------------------|---------------------------------------|
| JCS_GRAYSCALE          | truecolor (R=G=B=gray)                |
| JCS_RGB                | truecolor                              |
| JCS_YCbCr              | truecolor (libjpeg converts to RGB)    |
| JCS_CMYK / JCS_YCCK    | truecolor; CMYK→RGB via simple formula |
| JCS_UNKNOWN            | treated as RGB                         |

CMYK handling: if an Adobe APP14 marker is present the channels are inverted
(255−c, 255−m, 255−y, 255−k).  Conversion uses `(255−c)*(255−k)/255`, etc.
Resolution is copied from JFIF density markers (DPI or DPCM).

**Write:** always writes 3-channel RGB JPEG.  Palette images have their colour
map extracted as RGB rows.  Truecolor images write RGB rows directly, or
4-channel RGBA rows if 12-bit JSAMPLE (but 12-bit is discouraged).
Interlaced gd images produce progressive JPEGs.

### Read options — `gdJpegReadOptions`

```c
typedef struct {
    size_t struct_size;       // set by gdJpegReadOptionsInit()
    int ignore_warning;       // default 1 (ignore recoverable IJG warnings)
    unsigned int scale_num;   // default 1; scale numerator
    unsigned int scale_denom; // default 1; scale denominator  (scale_num/scale_denom)
    int dct_method;           // GD_JPEG_DCT_DEFAULT (0)
} gdJpegReadOptions;
```

- `ignore_warning`: if 0, recoverable libjpeg warnings are emitted via
  `gd_error()`; if 1, they are suppressed.
- `scale_num` / `scale_denom`: fraction applied to decoded dimensions.  The
  internal IJG scaler runs first; if the decoded dimensions don't match the
  requested fraction exactly, `gdImageScale()` performs a resize.  Values of 0
  are rejected.
- `dct_method`: one of `GD_JPEG_DCT_DEFAULT` (0), `GD_JPEG_DCT_SLOW` (1,
  JDCT_ISLOW), `GD_JPEG_DCT_FAST` (2, JDCT_IFAST), or `GD_JPEG_DCT_FLOAT` (3,
  JDCT_FLOAT).

### Write options — `gdJpegWriteOptions`

```c
typedef struct {
    size_t struct_size;          // set by gdJpegWriteOptionsInit()
    int quality;                 // default -1 (IJG default)
    int progressive;             // default 0; when negative gdImageGetInterlaced() is used
    int force_no_subsampling;    // default 0
    const gdImageMetadata *metadata; // default NULL
} gdJpegWriteOptions;
```

- `quality`: 0–100; −1 uses the IJG library default.
- `progressive`: ≥0 and non-zero → always progressive; <0 → progressive if
  `gdImageGetInterlaced()` is true.
- `force_no_subsampling`: if 1 (or `quality ≥ 90` independently) all chroma
  sampling factors are forced to 1×1 (4:4:4).
- `metadata`: embedded in COM/APP markers (EXIF, XMP → APP1; ICC → APP2
  segmented; IPTC → APP13).

### Info probe — `gdJpegInfo`

```c
typedef struct {
    int width, height;
    int bits_per_sample, components;
    int color_space;      // GD_JPEG_COLOR_SPACE_* constants
    int progressive;
    int density_unit;     // GD_JPEG_DENSITY_UNIT_NONE / DPI / DPCM
    int x_density, y_density;
    int has_exif, has_xmp, has_icc, has_iptc;
} gdJpegInfo;
```

Obtained via `gdJpegGetInfo()`, `gdJpegGetInfoCtx()`, `gdJpegGetInfoPtr()`.

### Public API

**Read (always truecolor)**:
- `gdImageCreateFromJpeg` / `gdImageCreateFromJpegEx`
- `gdImageCreateFromJpegPtr` / `gdImageCreateFromJpegPtrEx`
- `gdImageCreateFromJpegCtx` / `gdImageCreateFromJpegCtxEx`
- `gdImageCreateFromJpegPtrWithOptions` / `gdImageCreateFromJpegCtxWithOptions`
- `gdImageCreateFromJpegPtrWithMetadata` / `gdImageCreateFromJpegPtrExWithMetadata`
- `gdImageCreateFromJpegCtxWithMetadata` / `gdImageCreateFromJpegCtxExWithMetadata`

**Write**:
- `gdImageJpeg` / `gdImageJpegCtx`  (quality param)
- `gdImageJpegPtr` (quality param)
- `gdImageJpegPtrWithMetadata` / `gdImageJpegCtxWithMetadata`
- `gdImageJpegPtrWithMetadataNoSubsampling`
- `gdImageJpegWithOptions` / `gdImageJpegCtxWithOptions` / `gdImageJpegPtrWithOptions`

**Probe**: `gdJpegGetInfo` / `gdJpegGetInfoCtx` / `gdJpegGetInfoPtr`
**Version**: `gdJpegGetVersionString()`

---

## PNG (`src/gd_png.c`)

**Read:** preserves the original image type:
- Palette PNG (color type 3) → palette `gdImage`.
- Grayscale/grayscale+alpha → truecolor (gray expanded to R=G=B).
- Truecolor/truecolor+alpha → truecolor.
- tRNS chunk may add a transparent index (palette) or transparent color
  (truecolor). `gdImageSaveAlpha` set on truecolor RGBA images.

**Write:** palette-aware.  If the gd image is palette (≤256 colors) and has no
per-pixel alpha, writes PNG color type 3.  Truecolor images write as RGB or
RGBA depending on `saveAlphaFlag` / `transparent` color.  Interlaced gd images
produce Adam7 PNGs.

### Write options — `gdPngWriteOptions`

```c
typedef struct {
    size_t struct_size;
    int compression_level;         // 0–9, -1 = zlib default
    unsigned int filters;          // bitmask of GD_PNG_FILTER_* constants
    int compression_strategy;      // GD_PNG_COMPRESSION_STRATEGY_*
    const gdImageMetadata *metadata;
    unsigned int resolution_x;     // horizontal DPI, 0 = use gdImage value
    unsigned int resolution_y;     // vertical DPI, 0 = use gdImage value
} gdPngWriteOptions;
```

Filter constants:
- `GD_PNG_FILTER_AUTO` (0) — defer to libpng.
- `GD_PNG_FILTER_NONE`, `GD_PNG_FILTER_SUB`, `GD_PNG_FILTER_UP`,
  `GD_PNG_FILTER_AVERAGE`, `GD_PNG_FILTER_PAETH` — individual filter bits.
- `GD_PNG_FILTER_ALL` — bitwise OR of all five.

Compression strategy: `GD_PNG_COMPRESSION_STRATEGY_DEFAULT` (0),
`GD_PNG_COMPRESSION_STRATEGY_FILTERED`, `GD_PNG_COMPRESSION_STRATEGY_HUFFMAN_ONLY`,
`GD_PNG_COMPRESSION_STRATEGY_RLE`, `GD_PNG_COMPRESSION_STRATEGY_FIXED`.

### Info probe — `gdPngInfo`

```c
typedef struct {
    int width, height;
    int bit_depth, color_type;
    int has_alpha, has_transparency;
    int palette_entries;
    int interlace_method;
    int x_pixels_per_unit, y_pixels_per_unit, physical_unit;
    gdImageMetadata *metadata;
    int decoded_truecolor;
    int resolution_x, resolution_y;
} gdPngInfo;
```

`x_pixels_per_unit`, `y_pixels_per_unit`, and `physical_unit` expose the raw PNG
`pHYs` chunk values.  `resolution_x` and `resolution_y` are converted DPI values
when `physical_unit` is meters, or `-1` when no absolute DPI is available.

### Public API

**Read**:
- `gdImageCreateFromPng` / `gdImageCreateFromPngCtx` / `gdImageCreateFromPngPtr`
- `gdImageCreateFromPngCtxWithMetadata` / `gdImageCreateFromPngPtrWithMetadata`

**Write**:
- `gdImagePng` / `gdImagePngCtx`
- `gdImagePngEx` / `gdImagePngCtxEx` (compression level)
- `gdImagePngCtxWithMetadata` / `gdImagePngCtxExWithMetadata`
- `gdImagePngPtr` / `gdImagePngPtrEx`
- `gdImagePngPtrWithMetadata` / `gdImagePngPtrExWithMetadata`
- `gdImagePngWithOptions` / `gdImagePngCtxWithOptions` / `gdImagePngPtrWithOptions`

**Metadata injection**: `gdImageMetadataInjectPng`

**Probe**: `gdPngGetInfo` / `gdPngGetInfoCtx` / `gdPngGetInfoPtr`
**Version**: `gdPngGetVersionString()`

---

## GIF (`src/gd_gif_in.c`, `src/gd_gif_out.c`)

**Read (single frame):** `gdImageCreateFromGif*` returns a **palette** image
for the first frame.  Transparency is set to the GIF transparency index.

**Write (single frame):** `gdImageGif*` writes a palette-based GIF.  Truecolor
images are quantised internally by the legacy write API.

**Animation read:** `gdGifRead*` API reads GIF animations frame-by-frame with
metadata. `gdGifReadNextFrame` and `gdGifReadNextImage` return caller-owned
images that must be destroyed with `gdImageDestroy`.

**Animation write:** `gdImageGifAnimBegin` / `gdImageGifAnimAdd` /
`gdImageGifAnimEnd` legacy API writes GIF frames using frame-local colour
tables.

### Read info — `gdGifInfo`

```c
typedef struct {
    int width, height;
    int backgroundIndex;
    int globalColorTable;  // 1 if present
    int loopCount;
} gdGifInfo;
```

### Frame info — `gdGifFrameInfo`

```c
typedef struct {
    int frameIndex;
    int x, y;
    int width, height;
    int delay;            // in hundredths of a second
    int disposal;
    int transparentIndex;
    int localColorTable;  // 1 if present
    int interlace;
} gdGifFrameInfo;
```

Disposal constants: `GD_GIF_DISPOSAL_UNKNOWN` (0), `GD_GIF_DISPOSAL_NONE` (1),
`GD_GIF_DISPOSAL_RESTORE_BACKGROUND` (2), `GD_GIF_DISPOSAL_RESTORE_PREVIOUS` (3).

### Public API

**Read single frame** (palette):
- `gdImageCreateFromGif` / `gdImageCreateFromGifCtx` / `gdImageCreateFromGifPtr`

**Write single frame** (palette, quantised from truecolor):
- `gdImageGif` / `gdImageGifCtx` / `gdImageGifPtr`

**Animation read** (multi-frame):
- `gdGifIsAnimated` / `gdGifIsAnimatedCtx` / `gdGifIsAnimatedPtr`
- `gdGifReadOpen` / `gdGifReadOpenCtx` / `gdGifReadOpenPtr`
- `gdGifReadGetInfo`
- `gdGifReadNextFrame` (raw frames)
- `gdGifReadNextImage` (coalesced/rendered)
- `gdGifReadClose`

**Animation write** (legacy imperative API):
- `gdImageGifAnimBegin` / `gdImageGifAnimBeginCtx` / `gdImageGifAnimBeginPtr`
- `gdImageGifAnimAdd` / `gdImageGifAnimAddCtx` / `gdImageGifAnimAddPtr`
- `gdImageGifAnimEnd` / `gdImageGifAnimEndCtx` / `gdImageGifAnimEndPtr`

Parameters: `GlobalCM` (1 to include global colour table from the first frame),
`Loops` (−1 = infinite), `LocalCM` (1 for per-frame local colour table),
`LeftOfs`/`TopOfs` (frame position), `Delay` (hundredths of a second),
`Disposal` (one of `GD_GIF_DISPOSAL_*`), `previm` (previous frame for
disposal optimisation, can be NULL).

---

## BMP (`src/gd_bmp.c`)

**Read:**
- 1-bit BMP → palette image (white/black colours allocated).
- 4-bit, 8-bit palette BMP → palette image.
- 16-bit, 24-bit, 32-bit BMP → **truecolor** image.
- 32-bit with alpha bitmask → truecolor with `saveAlphaFlag=1`.
- RLE4 compressed 4-bit BMP → palette.
- RLE8 compressed 8-bit BMP → palette.
- 16-bit RGB565 mask: `0xF800`, `0x07E0`, `0x001F`.
- Non-standard bitmasks read from V4/V5 header fields.
- Top-down (negative height) BMPs handled.
- Resolution read from BMP headers (pixels per meter → DPI).

**Write:**
- Palette image with ≤2 colours → 1-bit BMP (b&w palette).
- Palette image with ≤16 colours → 4-bit BMP (RLE4 if requested).
- Palette image with ≤256 colours → 8-bit BMP (RLE8 if requested).
- Palette image with >256 colours → quantised to 8-bit.
- Truecolor image → 24-bit (RGB), 16-bit (RGB565 by default, or RGB555 with
  `GD_BMP_FLAG_RGB555`), or 32-bit (RGBA with alpha mask).
- 32-bit output uses alpha channel from `saveAlphaFlag`.
- V4 header written when 32-bit with alpha bitmask is present, or
  `GD_BMP_FLAG_FORCE_V4HDR` is set.
- RLE4/RLE8 output requires a seekable output stream and palette format.

### BMP bit depth and compression constants

```c
#define GD_BMP_COMPRESS_NONE  0   // BI_RGB
#define GD_BMP_COMPRESS_RLE4  2   // BI_RLE4 (4-bit only)
#define GD_BMP_COMPRESS_RLE8  3   // BI_RLE8 (8-bit only)

#define GD_BMP_FLAG_NONE         0
#define GD_BMP_FLAG_FORCE_V4HDR  1   // force V4 BMP header
#define GD_BMP_FLAG_RGB555       2   // RGB555 instead of RGB565 for 16 bpp
```

`GD_BMP_COMPRESS_RLE4` and `GD_BMP_COMPRESS_RLE8` require a seekable output
Ctx; if not available they silently fall back to uncompressed.

### Public API

**Read**:
- `gdImageCreateFromBmp` / `gdImageCreateFromBmpCtx` / `gdImageCreateFromBmpPtr`

**Write**:
- `gdImageBmp` / `gdImageBmpCtx` (legacy: auto bpp, compression flag)
- `gdImageBmpEx` / `gdImageBmpCtxEx` (explicit bpp, compression, flags)
- `gdImageBmpPtr` / `gdImageBmpPtrEx` (to memory)

---

## WebP (`src/gd_webp.c`)

**Read (single image):** Always returns a **truecolor** image.

- Uses `WebPDecodeARGB` first; falls back to `WebPAnimDecoder` if that fails
  (thus handles lossless and animated first-frame transparently).
- Palette images are not supported on decode (always truecolor).
- Alpha is always preserved (`saveAlphaFlag=1`).

**Write (single image):**
- Requires a truecolor image (palette images are rejected with error).
- `quality` parameter 0–100 for lossy, or `quality ≥ gdWebpLossless` (101) for
  lossless.
- Default quality when `-1`: 80.

**Animation read:** Two read modes:
1. `gdWebpReadNextFrame` — raw frame data (per-frame RGBA at frame offset).
2. `gdWebpReadNextImage` — coalesced frame (full-canvas RGBA rendered by the
   decoder, mimicking browser rendering).

**Animation write:** Imperative API with `gdWebpWriteOpen`,
`gdWebpWriteAddImage`, `gdWebpWriteClose` / `gdWebpWritePtrFinish`.  Frames
must all be truecolor and match the canvas size.  Palette frames are rejected.

### Info — `gdWebpInfo`

```c
typedef struct {
    int width, height;
    int frameCount;
    int loopCount;
    int backgroundColor;
    int formatFlags;        // raw WEBP_FF_FORMAT_FLAGS value
} gdWebpInfo;
```

### Frame info — `gdWebpFrameInfo`

```c
typedef struct {
    int frameIndex;
    int x, y;         // frame position on canvas
    int width, height;
    int duration;     // milliseconds
    int timestamp;    // accumulated ms
    int dispose;      // gdWebpDisposeNone / gdWebpDisposeBackground
    int blend;        // gdWebpBlendAlpha / gdWebpBlendNone
    int hasAlpha;
    int complete;
} gdWebpFrameInfo;
```

Dispose constants: `GD_WEBP_DISPOSE_NONE` (0), `GD_WEBP_DISPOSE_BACKGROUND` (1).
Blend constants: `GD_WEBP_BLEND_ALPHA` (0), `GD_WEBP_BLEND_NONE` (1).

### Write options — `gdWebpWriteOptions`

```c
typedef struct {
    int canvasWidth;      // 0 = use first frame width
    int canvasHeight;     // 0 = use first frame height
    int loopCount;        // 0 = infinite
    int backgroundColor;
    int quality;          // 0 = uses -1 (default 80)
    int lossless;         // 1 = force lossless
    int method;           // encoding method 0-6, -1 = default
    int minimizeSize;
    int kmin, kmax;       // distance parameters for lossy frames
    int allowMixed;       // allow lossless & lossy frames mixed
} gdWebpWriteOptions;
```

`quality ≥ gdWebpLossless` (101) always enables lossless mode.

### Public API

**Read single (truecolor)**:
- `gdImageCreateFromWebp` / `gdImageCreateFromWebpPtr` / `gdImageCreateFromWebpCtx`

**Write single (truecolor only)**:
- `gdImageWebp` (default quality)
- `gdImageWebpEx` / `gdImageWebpCtx` (quality)
- `gdImageWebpPtr` / `gdImageWebpPtrEx` (quality)

**Probe**: `gdWebpIsAnimated` / `gdWebpIsAnimatedCtx` / `gdWebpIsAnimatedPtr`

**Animation read**:
- `gdWebpReadOpen` / `gdWebpReadOpenCtx` / `gdWebpReadOpenPtr`
- `gdWebpReadGetInfo`
- `gdWebpReadNextFrame` (raw)
- `gdWebpReadNextImage` (coalesced)
- `gdWebpReadClose`

**Animation write**:
- `gdWebpWriteOpen` / `gdWebpWriteOpenCtx` / `gdWebpWriteOpenPtr`
- `gdWebpWriteAddImage` (duration in ms)
- `gdWebpWriteClose` (assemble and write)
- `gdWebpWritePtrFinish` (for memory writers)

---

## TIFF (`src/gd_tiff.c`)

**Read (single-page):** `gdImageCreateFromTiff*` reads the **first directory**
only.

Image type selection:
- Photometric palette (8-bit) → palette `gdImage`.
- 1-bit B/W (CCITT compressed assumed min-is-white, otherwise
  min-is-black/min-is-white) → palette (2-colour) via direct scanline/tile
  reader.
- **Everything else** (RGB, RGBA, gray, CMYK, YCbCr, CIELAB, separated planes,
  non-standard bps/spp, tiled non-1-bit) → `createFromTiffRgba()`, always
  returning a **truecolor** image.
- `TIFFReadRGBAImage` is used for these, which horizontally flips the image;
  the code flips it back internally.
- Associated/unassociated alpha is detected from the ExtraSamples tag.
  Associated alpha is un-premultiplied.

**Write (single image, legacy):** `gdImageTiffCtx` / `gdImageTiff`
- Truecolor → 24-bit RGB, or 32-bit RGBA if `saveAlphaFlag` is set.
- 2-colour palette → 1-bit B/W.
- Palette >2 colours → 8-bit palette (colour map written).
- Transparent index honoured on 24-bit → RGBA with full-transparent pixels.
- Compression always `COMPRESSION_ADOBE_DEFLATE`.

**Multi-page read:** `gdTiffRead*` API (re-reads the entire TIFF into memory,
uses `TIFFNumberOfDirectories` to count pages, iterates with
`TIFFSetDirectory` / `TIFFReadDirectory`).

**Multi-page write:** `gdTiffWrite*` API with options struct.

### Read info — `gdTiffInfo`

```c
typedef struct {
    int width, height;
    int pageCount;
    int bitsPerSample;
    int samplesPerPixel;
    int compression;     // GD_TIFF_COMPRESSION_* value
    int photometric;     // GD_TIFF_PHOTOMETRIC_* value
    float xResolution, yResolution;
    int resolutionUnit;  // GD_TIFF_RESUNIT_*
} gdTiffInfo;
```

### Page info — `gdTiffPageInfo`

```c
typedef struct {
    int pageIndex;
    int width, height;
    int bitsPerSample, samplesPerPixel;
    int compression;
    int photometric;
    int planar;          // GD_TIFF_PLANARCONFIG_CONTIG / SEPARATE
    int hasAlpha;
    int isTiled;
    float xResolution, yResolution;
    int resolutionUnit;
} gdTiffPageInfo;
```

### Write options — `gdTiffWriteOptions`

```c
typedef struct {
    int bitDepth;         // 1, 8, or 16; default 8
    int colorspace;       // GD_TIFF_RGB / GD_TIFF_RGBA / GD_TIFF_GRAY / GD_TIFF_BILEVEL
    int compression;      // GD_TIFF_COMPRESSION_*  (default ADOBE_DEFLATE)
    int jpegQuality;      // 1-100, only for JPEG compression; default 75
    int minIsWhite;       // for GD_TIFF_GRAY / GD_TIFF_BILEVEL: 1 = min-is-white photometric
    int resolutionUnit;   // GD_TIFF_RESUNIT_* (default RESUNIT_INCH)
    float xResolution;    // default 72.0
    float yResolution;    // default 72.0
    int alphaType;        // GD_TIFF_ALPHA_UNASSOCIATED / GD_TIFF_ALPHA_ASSOCIATED
} gdTiffWriteOptions;
```

Colorspace constants:
- `GD_TIFF_RGB` (1)
- `GD_TIFF_RGBA` (2)
- `GD_TIFF_GRAY` (3)
- `GD_TIFF_BILEVEL` (4)

Compression constants:
- `GD_TIFF_COMPRESSION_NONE` (1)
- `GD_TIFF_COMPRESSION_CCITT_RLE` (2)
- `GD_TIFF_COMPRESSION_CCITT_FAX3` (3) — 1-bit only
- `GD_TIFF_COMPRESSION_CCITT_FAX4` (4) — 1-bit only
- `GD_TIFF_COMPRESSION_LZW` (5)
- `GD_TIFF_COMPRESSION_JPEG` (7) — 8-bit only
- `GD_TIFF_COMPRESSION_ADOBE_DEFLATE` (8) — default
- `GD_TIFF_COMPRESSION_DEFLATE` (32946)
- `GD_TIFF_COMPRESSION_PACKBITS` (32773)

Photometric constants (for reading/info):
- `GD_TIFF_PHOTOMETRIC_MINISWHITE` (0)
- `GD_TIFF_PHOTOMETRIC_MINISBLACK` (1)
- `GD_TIFF_PHOTOMETRIC_RGB` (2)
- `GD_TIFF_PHOTOMETRIC_PALETTE` (3)
- `GD_TIFF_PHOTOMETRIC_TRANSPARENCY_MASK` (4)
- `GD_TIFF_PHOTOMETRIC_SEPARATED` (5)
- `GD_TIFF_PHOTOMETRIC_YCBCR` (6)
- `GD_TIFF_PHOTOMETRIC_CIELAB` (8)

Resolution unit constants:
- `GD_TIFF_RESUNIT_NONE` (1)
- `GD_TIFF_RESUNIT_INCH` (2)
- `GD_TIFF_RESUNIT_CENTIMETER` (3)

Alpha type:
- `GD_TIFF_ALPHA_UNASSOCIATED` (1) — default
- `GD_TIFF_ALPHA_ASSOCIATED` (2)

### Public API

**Read (single-page)**:
- `gdImageCreateFromTiff` / `gdImageCreateFromTiffCtx` / `gdImageCreateFromTiffPtr`

**Write (single-page, legacy)**:
- `gdImageTiff` / `gdImageTiffCtx` / `gdImageTiffPtr`

**Multi-page probe**: `gdTiffIsMultiPage` / `gdTiffIsMultiPageCtx` / `gdTiffIsMultiPagePtr`

**Multi-page read**:
- `gdTiffReadOpen` / `gdTiffReadOpenCtx` / `gdTiffReadOpenPtr`
- `gdTiffReadGetInfo`
- `gdTiffReadNextImage` (next page → gdImage + `gdTiffPageInfo`)
- `gdTiffReadClose`

**Multi-page write**:
- `gdTiffWriteOpen` / `gdTiffWriteOpenCtx` / `gdTiffWriteOpenPtr` (pass options)
- `gdTiffWriteAddImage` (adds one page)
- `gdTiffWriteClose` (finalise)
- `gdTiffWritePtrFinish` (for memory writers)

---

## JPEG XL (`src/gd_jxl.c`)

**Read (still):** Always **truecolor** with alpha.

- Requests RGBA U8 output with sRGB colour profile.
- Uses the default JXL CMS for colour space conversion.
- Alpha channel extracted from `alpha_bits > 0` in basic info.
- JXL alpha (0=transparent, 255=opaque) converted to gd alpha (0=opaque,
  127=transparent).

**Write (still):**
- Default: lossy, distance 1.0, effort 7.
- Palette images are automatically converted to truecolor before encoding.
- Alpha presence is scanned per-pixel; if any pixel has non-opaque alpha,
  includes an alpha extra channel.
- If no alpha, strips the 4th byte and writes 3-channel RGB.
- Lossless: `uses_original_profile=JXL_TRUE` and `JxlEncoderSetFrameLossless`.
- Lossy: `JxlEncoderSetFrameDistance`.

**Animation read:** Two modes:
1. Coalesced (default) — `JxlDecoderSetCoalescing(JXL_TRUE)`, each frame is
   full-canvas.  Use `gdJxlReadNextImage`.
2. Raw (non-coalesced) — `JxlDecoderSetCoalescing(JXL_FALSE)`, each frame has
   crop offsets and blend info.  Use `gdJxlReadNextFrame` with `gdJxlFrameInfo`.

**Multi-image write:** `gdJxlWriteOptions` controls canvas size, lossless/lossy
settings, effort, and loop count.  A zero canvas dimension is inferred from the
first frame.  Frames must be truecolor and match the resolved canvas size.
`delay_ms` is in milliseconds; `tps_numerator=1000, tps_denominator=1`
(1 tick = 1 ms).  `loopCount=0` means loop forever.

### Reader info — `gdJxlInfo`

```c
typedef struct {
    int width;
    int height;
    int animated;
    int loop_count;
} gdJxlInfo;
```

### Frame info — `gdJxlFrameInfo`

```c
typedef struct {
    int delay_ms;
    int x_offset, y_offset;
    int width, height;
    int blend_mode;   // GD_JXL_BLEND_*
    int is_last;
} gdJxlFrameInfo;
```

Blend constants:
- `GD_JXL_BLEND_REPLACE` (0)
- `GD_JXL_BLEND_ADD` (1)
- `GD_JXL_BLEND_BLEND` (2)
- `GD_JXL_BLEND_MULADD` (3)
- `GD_JXL_BLEND_MUL` (4)

### Read/write options

```c
typedef struct {
    size_t struct_size;
    int coalesced;
} gdJxlReadOptions;

typedef struct {
    size_t struct_size;
    int canvasWidth, canvasHeight;
    int lossless;
    float distance;
    int effort;
    int loopCount;
} gdJxlWriteOptions;
```

### Public API

**Read still (truecolor)**:
- `gdImageCreateFromJxl` / `gdImageCreateFromJxlPtr` / `gdImageCreateFromJxlCtx`

**Write still**:
- `gdImageJxl` (default: lossy, distance 1.0, effort 7)
- `gdImageJxlEx` (params: `lossless`, `distance`, `effort`)
- `gdImageJxlPtr` / `gdImageJxlPtrEx`
- `gdImageJxlCtx` / `gdImageJxlCtxEx`

**Multi-image read**:
- `gdJxlReadOptionsInit`
- `gdJxlReadOpen` / `gdJxlReadOpenCtx` / `gdJxlReadOpenPtr`
- `gdJxlReadGetInfo`
- `gdJxlReadNextImage` (coalesced full-canvas images)
- `gdJxlReadNextFrame` (raw frame rectangles + `gdJxlFrameInfo`)
- `gdJxlReadClose`

**Multi-image write**:
- `gdJxlWriteOptionsInit`
- `gdJxlWriteOpen` / `gdJxlWriteOpenCtx` / `gdJxlWriteOpenPtr`
- `gdJxlWriteAddImage`
- `gdJxlWriteClose` / `gdJxlWritePtrFinish`

---

## AVIF (`src/gd_avif.c`)

**Read:** Always **truecolor** RGBA.  Reads the first image from a sequence
(ignoring subsequent frames).  Strictness relaxed to allow missing 'pixi'
property (AVIF ≥ 0.9.1).  Alpha from AVIF 8-bit converted to gd 7-bit.

**Write:** Requires **truecolor** (palette rejected).  Quality 0–100 (higher =
better, 100 = lossless).  Speed 0–10 (AVIF_SPEED_SLOWEST to AVIF_SPEED_FASTEST).

Internal defaults:
- Chroma subsampling: 4:2:0 below quality 90; 4:4:4 at/above 90.
- Quantizer: derived from quality (quality→quantizer conversion).
- Default quality: 30 quantizer (equivalent to ~70 quality).
- Default speed: 6.
- Lossless at quality=100: uses `AVIF_MATRIX_COEFFICIENTS_IDENTITY`.
- Tiles computed from image area (min tile area 512×512, max 8 tiles, max 64 threads).

### Public API

**Read (truecolor)**:
- `gdImageCreateFromAvif` / `gdImageCreateFromAvifPtr` / `gdImageCreateFromAvifCtx`

**Write**:
- `gdImageAvif` (default quality/speed)
- `gdImageAvifEx` (quality, speed)
- `gdImageAvifPtr` / `gdImageAvifPtrEx`
- `gdImageAvifCtx`

---

## HEIF (`src/gd_heif.c`)

**Read:** Always **truecolor** RGBA.  Accepts AVIF, MIF1, HEIC, HEIX brands
(any of them).  HDR converted to 8-bit; transformations ignored.  Alpha
converted to gd 7-bit.

**Write:** Requires **truecolor** (palette rejected).  Output is RGBA 32-bit
per channel interleaved.  Quality 0–100 (lossy), 200 (lossless), or −1
(default 80).  Two codecs: HEVC (default) or AV1.  Three chroma subsampling
modes.

### Codec and chroma constants

```c
typedef enum {
    GD_HEIF_CODEC_UNKNOWN = 0,
    GD_HEIF_CODEC_HEVC,     // = 1
    GD_HEIF_CODEC_AV1 = 4,
} gdHeifCodec;

#define GD_HEIF_CHROMA_420 "420"
#define GD_HEIF_CHROMA_422 "422"
#define GD_HEIF_CHROMA_444 "444"
```

Chroma values are `const char*` strings.

### Public API

**Read (truecolor)**:
- `gdImageCreateFromHeif` / `gdImageCreateFromHeifPtr` / `gdImageCreateFromHeifCtx`

**Write**:
- `gdImageHeif` (default: quality -1, HEVC, 4:4:4)
- `gdImageHeifEx` (quality, codec, chroma)
- `gdImageHeifPtr` / `gdImageHeifPtrEx`
- `gdImageHeifCtx`

---

## QOI (`src/gd_qoi.c`)

**Read:** Always **truecolor** with RGBA.  `saveAlphaFlag=1`.

**Write:**
- Palette images convert palette indices into RGBA (colour map lookup).
  Truecolor images extract RGB/RGBA per pixel.
- Output always includes RGBA (with 8-bit alpha in QOI format).

### Colorspace constants

```c
enum { GD_QOI_SRGB = 0, GD_QOI_LINEAR = 1 };
```

### Public API

**Read (truecolor)**:
- `gdImageCreateFromQoi` / `gdImageCreateFromQoiCtx` / `gdImageCreateFromQoiPtr`
- `gdImageCreateFromQoiCtxWithMetadata` / `gdImageCreateFromQoiPtrWithMetadata`
- `gdImageCreateFromQoiSource`

**Write**:
- `gdImageQoi` (default: sRGB)
- `gdImageQoiEx` (colorspace param: `GD_QOI_SRGB` or `GD_QOI_LINEAR`)
- `gdImageQoiCtx` / `gdImageQoiCtxEx`
- `gdImageQoiCtxWithMetadata` / `gdImageQoiCtxExWithMetadata`
- `gdImageQoiPtr` / `gdImageQoiPtrEx`
- `gdImageQoiPtrWithMetadata` / `gdImageQoiPtrExWithMetadata`
- `gdImageQoiToSink`
- `gdImageMetadataInjectQoi`

---

## TGA (`src/gd_tga.c`)

**Read only.** Always **truecolor**.

| TGA image type       | Depth supported            | Output            |
|----------------------|----------------------------|-------------------|
| TGA_TYPE_INDEXED (1) | 8-bit (colormap 15/16/24/32) | truecolor via colormap lookup |
| TGA_TYPE_RGB (2)     | 16-bit, 24-bit, 32-bit     | truecolor          |
| TGA_TYPE_GREYSCALE (3)| 8-bit                      | truecolor (R=G=B=gray) |
| RLE variants (9,10,11)| same depths as above    | truecolor          |

- 16-bit: 5-bit per channel scaled to 8-bit; alpha bit (bit 15) → opaque or
  transparent via `gdTrueColorAlpha`.
- 32-bit: RGBA 8-bit with alpha converted to gd 7-bit.
- Alpha is initially saved (`saveAlphaFlag=1`); if the optional TGA 2.0 footer
  indicates attribute type ≠ 3 and ≠ 4, alpha is stripped.
- Image flipped according to header descriptor bits 4 and 5.

**No write support.**

### Public API

**Read (truecolor)**:
- `gdImageCreateFromTga` / `gdImageCreateFromTgaPtr` / `gdImageCreateFromTgaCtx`

---

## WBMP (`src/wbmp.c`, `src/gd_wbmp.c`)

Wireless Bitmap — black-and-white uncompressed 1-bit format.

**Read:** Returns a **palette** image with 2 colours: white (255,255,255) and
black (0,0,0).  WBMP white pixel (1) → background (white), black (0) →
foreground.

**Write:** Accepts any gd image (palette or truecolor).  Any pixel matching the
foreground colour index `fg` becomes black; all others become white (i.e.
background).

### Public API

**Read (palette)**:
- `gdImageCreateFromWBMP` / `gdImageCreateFromWBMPCtx` / `gdImageCreateFromWBMPPtr`

**Write**:
- `gdImageWBMP` (`fg` foreground index)
- `gdImageWBMPCtx` (`fg` foreground index)
- `gdImageWBMPPtr` (`fg` foreground index + size output)

---

## XBM (`src/gd_xbm.c`)

X11/ X10 bitmap — black-and-white 1-bit text format (hex-encoded C array).

**Read:** Parses `#define width/height` and `static char/short …_bits[]`
definitions (X11: char, X10: short).  Returns a **palette** image with 2
colours: foreground (black, 0,0,0) and background (white, 255,255,255).  X11
bitmaps encode 8 pixels per byte (1=foreground).  X10 encodes 16 pixels per
short.

**Write:** Outputs X11 format (`static unsigned char …_bits[]`).  Pixels
matching the foreground colour index `fg` become 1-bits; others become 0-bits.
Output includes `#define` width/height macros and hex values formatted at 12
per line.

### Public API

**Read (palette)**:
- `gdImageCreateFromXbm`

**Write**:
- `gdImageXbmCtx` (params: `file_name` for C identifier prefix, `fg` foreground colour index)

---

## XPM (`src/gdxpm.c`)

X PixMap — a textual colour image format parsed by `libXpm`.

**Read only.** Returns a **palette** image (created via `gdImageCreate`).
Colours are allocated from the XPM colour table:

- `"None"` → transparent colour (`gdImageColorTransparent`).
- Hex colours (formats: `#RGB`, `#RRGGBB`, `#RRRGGGBBB`, `#RRRRGGGGBBBB`).
- X11 named colours via `gdColorMapLookup`.
- Colours resolved via `gdImageColorResolve`.

No write support.  Uses filename, not FILE pointer.

### Public API

**Read (palette)**:
- `gdImageCreateFromXpm(filename)`  (requires `HAVE_LIBXPM`, uses filename string)

---

## TIFF Write API (multi-page) — continued

The write API also supports multi-page writing.  `gdTiffWriteOpen` starts a
session; each call to `gdTiffWriteAddImage` appends a new page (TIFF
directory); `gdTiffWriteClose` finishes.  All pages share the same write
options.  Palette images are quantized/converted according to the options'
colorspace/bitDepth settings.

---

## `gdImageMetadata` and profile injection

Several codecs support a `const gdImageMetadata *metadata` parameter on write
(specifically JPEG, PNG, QOI).  Metadata can contain named byte profiles:

| Profile name | JPEG placement   | PNG placement | QOI handling |
|--------------|------------------|---------------|---------------|
| `"exif"`     | APP1 marker      | metadata injection | metadata injection |
| `"xmp"`      | APP1 marker      | same         | same |
| `"icc"`      | APP2 segmented   | same         | same |
| `"iptc"`     | APP13 marker     | same         | same |

PNG and QOI metadata injection use `gdImageMetadataInjectPng` and
`gdImageMetadataInjectQoi` respectively.

JPEG, PNG, and QOI also support reading metadata (`gdImageMetadata` output)
via `gdImageCreateFrom…WithMetadata` variants.  TIFF and AVIF read metadata
from their native structures rather than from `gdImageMetadata`.
