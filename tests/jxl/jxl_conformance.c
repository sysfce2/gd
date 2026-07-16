#include "gd.h"
#include "gdtest.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { JXL_EXPECT_VALID, JXL_EXPECT_INVALID } jxl_expectation;

static unsigned char *read_file(FILE *fp, int *size) {
	long len;
	unsigned char *data;

	*size = 0;
	if (fseek(fp, 0, SEEK_END) != 0) {
		return NULL;
	}
	len = ftell(fp);
	if (len < 0 || len > INT_MAX || fseek(fp, 0, SEEK_SET) != 0) {
		return NULL;
	}
	data = (unsigned char *)malloc(len > 0 ? (size_t)len : 1);
	if (data == NULL) {
		return NULL;
	}
	if (len > 0 && fread(data, 1, (size_t)len, fp) != (size_t)len) {
		free(data);
		return NULL;
	}
	*size = (int)len;
	return data;
}

static void assert_valid_file(const char *directory, const char *filename) {
	unsigned char *data;
	gdJxlReadPtr reader;
	gdImagePtr image;
	int delay;
	int result;
	int frames = 0;
	int size;
	FILE *fp;

	fp = gdTestFileOpenX("conformance", "jxl", "conformance", directory,
					 filename, NULL);
	gdTestAssertMsg(fp != NULL, "cannot open JXL corpus file: %s\n", filename);
	if (fp == NULL) {
		return;
	}
	data = read_file(fp, &size);
	fclose(fp);
	gdTestAssertMsg(data != NULL, "cannot read JXL corpus file: %s\n",
					filename);
	if (data == NULL) {
		return;
	}

	image = gdImageCreateFromJxlPtr(size, data);
	gdTestAssertMsg(image != NULL, "valid JXL failed to decode: %s\n", filename);
	if (image != NULL) {
		gdTestAssertMsg(gdImageSX(image) > 0 && gdImageSY(image) > 0,
						"decoded JXL has invalid dimensions: %s\n", filename);
		gdImageDestroy(image);
	}

	reader = gdJxlReadOpenPtr(size, data, NULL);
	gdTestAssertMsg(reader != NULL,
					"animation reader rejected valid JXL: %s\n", filename);
	if (reader != NULL) {
		while ((result = gdJxlReadNextImage(reader, &delay, &image)) == 1) {
			gdTestAssertMsg(gdImageSX(image) > 0 && gdImageSY(image) > 0,
							"JXL frame has invalid dimensions: %s\n", filename);
			gdTestAssertMsg(delay >= 0,
							"JXL frame has negative duration: %s\n", filename);
			frames++;
			gdImageDestroy(image);
		}
		gdTestAssertMsg(result == 0, "JXL frame iteration failed: %s\n", filename);
		gdTestAssertMsg(frames > 0, "valid JXL yielded no frames: %s\n",
						filename);
		gdJxlReadClose(reader);
	}

	free(data);
}

static void assert_invalid_file(const char *directory, const char *filename) {
	unsigned char *data;
	gdJxlReadPtr reader;
	gdImagePtr image;
	int delay = -1;
	int size;
	FILE *fp;

	fp = gdTestFileOpenX("conformance", "jxl", "conformance", directory,
					 filename, NULL);
	gdTestAssertMsg(fp != NULL, "cannot open invalid JXL file: %s\n", filename);
	if (fp == NULL) {
		return;
	}
	data = read_file(fp, &size);
	fclose(fp);
	gdTestAssertMsg(data != NULL, "cannot read invalid JXL file: %s\n",
					filename);
	if (data == NULL) {
		return;
	}

	image = gdImageCreateFromJxlPtr(size, data);
	gdTestAssertMsg(image == NULL, "invalid JXL decoded successfully: %s\n",
					filename);
	if (image != NULL) {
		gdImageDestroy(image);
	}

	reader = gdJxlReadOpenPtr(size, data, NULL);
	if (reader != NULL) {
		gdTestAssert(gdJxlReadNextImage(reader, &delay, &image) != 1);
		gdTestAssertMsg(image == NULL,
						"invalid JXL animation yielded a frame: %s\n", filename);
		if (image != NULL) {
			gdImageDestroy(image);
		}
		gdJxlReadClose(reader);
	}

	free(data);
}

typedef struct {
	const char *directory;
	const char *filename;
	jxl_expectation expectation;
} jxl_case;

static const jxl_case cases[] = {
	{"conformance", "alpha_nonpremultiplied.jxl", JXL_EXPECT_VALID},
	{"conformance", "alpha_premultiplied.jxl", JXL_EXPECT_VALID},
	{"conformance", "alpha_triangles.jxl", JXL_EXPECT_VALID},
	{"conformance", "animation_icos4d.jxl", JXL_EXPECT_VALID},
	{"conformance", "animation_icos4d_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "animation_newtons_cradle.jxl", JXL_EXPECT_VALID},
	{"conformance", "animation_spline.jxl", JXL_EXPECT_VALID},
	{"conformance", "animation_spline_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "bench_oriented_brg.jxl", JXL_EXPECT_VALID},
	{"conformance", "bench_oriented_brg_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "bicycles.jxl", JXL_EXPECT_VALID},
	{"conformance", "bike.jxl", JXL_EXPECT_VALID},
	{"conformance", "bike_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "blendmodes.jxl", JXL_EXPECT_VALID},
	{"conformance", "blendmodes_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "cafe.jxl", JXL_EXPECT_VALID},
	{"conformance", "cafe_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "cmyk_layers.jxl", JXL_EXPECT_VALID},
	{"conformance", "delta_palette.jxl", JXL_EXPECT_VALID},
	{"conformance", "grayscale.jxl", JXL_EXPECT_VALID},
	{"conformance", "grayscale_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "grayscale_jpeg.jxl", JXL_EXPECT_VALID},
	{"conformance", "grayscale_jpeg_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "grayscale_public_university.jxl", JXL_EXPECT_VALID},
	{"conformance", "lossless_pfm.jxl", JXL_EXPECT_VALID},
	{"conformance", "lz77_flower.jxl", JXL_EXPECT_VALID},
	{"conformance", "noise.jxl", JXL_EXPECT_VALID},
	{"conformance", "noise_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "opsin_inverse.jxl", JXL_EXPECT_VALID},
	{"conformance", "opsin_inverse_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "patches.jxl", JXL_EXPECT_VALID},
	{"conformance", "patches_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "patches_lossless.jxl", JXL_EXPECT_VALID},
	{"conformance", "progressive.jxl", JXL_EXPECT_VALID},
	{"conformance", "progressive_5.jxl", JXL_EXPECT_VALID},
	{"conformance", "spot.jxl", JXL_EXPECT_VALID},
	{"conformance", "sunset_logo.jxl", JXL_EXPECT_VALID},
	{"conformance", "upsampling.jxl", JXL_EXPECT_VALID},
	{"conformance", "upsampling_5.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "3x3_srgb_lossless.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "3x3_srgb_lossy.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "8x8_noise.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "basic.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "cropped_traffic_light.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "has_permutation.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "large_header.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "multiple_lf_420.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "oddsize_ups.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "patch_y_out_of_bounds.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "squeeze_edge.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "squeeze_empty_residual.jxl", JXL_EXPECT_VALID},
	{"edge-cases", "tree_max_property_20.jxl", JXL_EXPECT_VALID},
	{"features", "3x3_jpeg_recompression.jxl", JXL_EXPECT_VALID},
	{"features", "3x3a_srgb_lossless.jxl", JXL_EXPECT_VALID},
	{"features", "3x3a_srgb_lossy.jxl", JXL_EXPECT_VALID},
	{"features", "alpha_distance_0_0.jxl", JXL_EXPECT_VALID},
	{"features", "alpha_distance_0_5.jxl", JXL_EXPECT_VALID},
	{"features", "alpha_distance_1_0.jxl", JXL_EXPECT_VALID},
	{"features", "alpha_distance_2_0.jxl", JXL_EXPECT_VALID},
	{"features", "animation_distance_0_0.jxl", JXL_EXPECT_VALID},
	{"features", "animation_distance_1_0.jxl", JXL_EXPECT_VALID},
	{"features", "animation_distance_3_0.jxl", JXL_EXPECT_VALID},
	{"features", "animation_effort_3.jxl", JXL_EXPECT_VALID},
	{"features", "animation_effort_7.jxl", JXL_EXPECT_VALID},
	{"features", "animation_effort_9.jxl", JXL_EXPECT_VALID},
	{"features", "animation_frame_indexed.jxl", JXL_EXPECT_VALID},
	{"features", "animation_modular.jxl", JXL_EXPECT_VALID},
	{"features", "animation_progressive.jxl", JXL_EXPECT_VALID},
	{"features", "animation_with_patches.jxl", JXL_EXPECT_VALID},
	{"features", "bitdepth_10.jxl", JXL_EXPECT_VALID},
	{"features", "bitdepth_12.jxl", JXL_EXPECT_VALID},
	{"features", "bitdepth_16.jxl", JXL_EXPECT_VALID},
	{"features", "bitdepth_8.jxl", JXL_EXPECT_VALID},
	{"features", "colorspace_DisplayP3.jxl", JXL_EXPECT_VALID},
	{"features", "colorspace_Rec2100HLG.jxl", JXL_EXPECT_VALID},
	{"features", "colorspace_Rec2100PQ.jxl", JXL_EXPECT_VALID},
	{"features", "colorspace_sRGB.jxl", JXL_EXPECT_VALID},
	{"features", "compress_boxes_0.jxl", JXL_EXPECT_VALID},
	{"features", "compress_boxes_1.jxl", JXL_EXPECT_VALID},
	{"features", "container_forced.jxl", JXL_EXPECT_VALID},
	{"features", "custom_icc_profile.jxl", JXL_EXPECT_VALID},
	{"features", "distance_0_0.jxl", JXL_EXPECT_VALID},
	{"features", "distance_0_5.jxl", JXL_EXPECT_VALID},
	{"features", "distance_10_0.jxl", JXL_EXPECT_VALID},
	{"features", "distance_1_0.jxl", JXL_EXPECT_VALID},
	{"features", "distance_2_0.jxl", JXL_EXPECT_VALID},
	{"features", "distance_5_0.jxl", JXL_EXPECT_VALID},
	{"features", "dots_0.jxl", JXL_EXPECT_VALID},
	{"features", "dots_1.jxl", JXL_EXPECT_VALID},
	{"features", "ec_resampling_1x.jxl", JXL_EXPECT_VALID},
	{"features", "ec_resampling_2x.jxl", JXL_EXPECT_VALID},
	{"features", "ec_resampling_4x.jxl", JXL_EXPECT_VALID},
	{"features", "effort_1.jxl", JXL_EXPECT_VALID},
	{"features", "effort_10.jxl", JXL_EXPECT_VALID},
	{"features", "effort_3.jxl", JXL_EXPECT_VALID},
	{"features", "effort_5.jxl", JXL_EXPECT_VALID},
	{"features", "effort_7.jxl", JXL_EXPECT_VALID},
	{"features", "effort_9.jxl", JXL_EXPECT_VALID},
	{"features", "epf_level_0.jxl", JXL_EXPECT_VALID},
	{"features", "epf_level_1.jxl", JXL_EXPECT_VALID},
	{"features", "epf_level_2.jxl", JXL_EXPECT_VALID},
	{"features", "epf_level_3.jxl", JXL_EXPECT_VALID},
	{"features", "extra_channels.jxl", JXL_EXPECT_VALID},
	{"features", "faster_decoding_0.jxl", JXL_EXPECT_VALID},
	{"features", "faster_decoding_1.jxl", JXL_EXPECT_VALID},
	{"features", "faster_decoding_2.jxl", JXL_EXPECT_VALID},
	{"features", "faster_decoding_3.jxl", JXL_EXPECT_VALID},
	{"features", "faster_decoding_4.jxl", JXL_EXPECT_VALID},
	{"features", "gaborish_0.jxl", JXL_EXPECT_VALID},
	{"features", "gaborish_1.jxl", JXL_EXPECT_VALID},
	{"features", "grayscale_patches_modular.jxl", JXL_EXPECT_VALID},
	{"features", "grayscale_patches_var_dct.jxl", JXL_EXPECT_VALID},
	{"features", "green_queen_modular_e3.jxl", JXL_EXPECT_VALID},
	{"features", "green_queen_vardct_e3.jxl", JXL_EXPECT_VALID},
	{"features", "group_order_0.jxl", JXL_EXPECT_VALID},
	{"features", "group_order_1.jxl", JXL_EXPECT_VALID},
	{"features", "has_permutation_with_container.jxl", JXL_EXPECT_VALID},
	{"features", "hdr_hlg_test.jxl", JXL_EXPECT_VALID},
	{"features", "hdr_pq_test.jxl", JXL_EXPECT_VALID},
	{"features", "intensity_target_10000nits.jxl", JXL_EXPECT_VALID},
	{"features", "intensity_target_1000nits.jxl", JXL_EXPECT_VALID},
	{"features", "intensity_target_100nits.jxl", JXL_EXPECT_VALID},
	{"features", "intensity_target_4000nits.jxl", JXL_EXPECT_VALID},
	{"features", "keep_invisible_0.jxl", JXL_EXPECT_VALID},
	{"features", "keep_invisible_1.jxl", JXL_EXPECT_VALID},
	{"features", "modular_colorspace_0.jxl", JXL_EXPECT_VALID},
	{"features", "modular_colorspace_1.jxl", JXL_EXPECT_VALID},
	{"features", "modular_colorspace_6.jxl", JXL_EXPECT_VALID},
	{"features", "modular_group_size_0.jxl", JXL_EXPECT_VALID},
	{"features", "modular_group_size_1.jxl", JXL_EXPECT_VALID},
	{"features", "modular_group_size_2.jxl", JXL_EXPECT_VALID},
	{"features", "modular_group_size_3.jxl", JXL_EXPECT_VALID},
	{"features", "modular_predictor_0.jxl", JXL_EXPECT_VALID},
	{"features", "modular_predictor_1.jxl", JXL_EXPECT_VALID},
	{"features", "modular_predictor_14.jxl", JXL_EXPECT_VALID},
	{"features", "modular_predictor_15.jxl", JXL_EXPECT_VALID},
	{"features", "modular_predictor_2.jxl", JXL_EXPECT_VALID},
	{"features", "modular_predictor_5.jxl", JXL_EXPECT_VALID},
	{"features", "modular_predictor_6.jxl", JXL_EXPECT_VALID},
	{"features", "multiple_layers_noise_spline.jxl", JXL_EXPECT_VALID},
	{"features", "no_container.jxl", JXL_EXPECT_VALID},
	{"features", "noise_0.jxl", JXL_EXPECT_VALID},
	{"features", "noise_1.jxl", JXL_EXPECT_VALID},
	{"features", "orientation1_identity.jxl", JXL_EXPECT_VALID},
	{"features", "orientation2_flip_horizontal.jxl", JXL_EXPECT_VALID},
	{"features", "orientation3_rotate_180.jxl", JXL_EXPECT_VALID},
	{"features", "orientation4_flip_vertical.jxl", JXL_EXPECT_VALID},
	{"features", "orientation5_transpose.jxl", JXL_EXPECT_VALID},
	{"features", "orientation6_rotate_90_cw.jxl", JXL_EXPECT_VALID},
	{"features", "orientation7_anti_transpose.jxl", JXL_EXPECT_VALID},
	{"features", "orientation8_rotate_90_ccw.jxl", JXL_EXPECT_VALID},
	{"features", "patches_0.jxl", JXL_EXPECT_VALID},
	{"features", "patches_1.jxl", JXL_EXPECT_VALID},
	{"features", "photon_noise_iso100.jxl", JXL_EXPECT_VALID},
	{"features", "photon_noise_iso1600.jxl", JXL_EXPECT_VALID},
	{"features", "photon_noise_iso400.jxl", JXL_EXPECT_VALID},
	{"features", "photon_noise_iso6400.jxl", JXL_EXPECT_VALID},
	{"features", "premultiply_0.jxl", JXL_EXPECT_VALID},
	{"features", "premultiply_1.jxl", JXL_EXPECT_VALID},
	{"features", "progressive_ac.jxl", JXL_EXPECT_VALID},
	{"features", "progressive_basic.jxl", JXL_EXPECT_VALID},
	{"features", "progressive_dc_0.jxl", JXL_EXPECT_VALID},
	{"features", "progressive_dc_1.jxl", JXL_EXPECT_VALID},
	{"features", "progressive_dc_2.jxl", JXL_EXPECT_VALID},
	{"features", "qprogressive_ac.jxl", JXL_EXPECT_VALID},
	{"features", "resampling_1x.jxl", JXL_EXPECT_VALID},
	{"features", "resampling_2x.jxl", JXL_EXPECT_VALID},
	{"features", "resampling_4x.jxl", JXL_EXPECT_VALID},
	{"features", "resampling_8x.jxl", JXL_EXPECT_VALID},
	{"features", "spline_on_first_frame.jxl", JXL_EXPECT_VALID},
	{"features", "splines.jxl", JXL_EXPECT_VALID},
	{"features", "squeeze_alpha.jxl", JXL_EXPECT_VALID},
	{"features", "squeeze_responsive_0.jxl", JXL_EXPECT_VALID},
	{"features", "squeeze_responsive_1.jxl", JXL_EXPECT_VALID},
	{"features", "stripped_metadata.jxl", JXL_EXPECT_VALID},
	{"features", "with_exif.jxl", JXL_EXPECT_VALID},
	{"features", "with_exif_and_xmp.jxl", JXL_EXPECT_VALID},
	{"features", "with_icc.jxl", JXL_EXPECT_VALID},
	{"features", "with_preview.jxl", JXL_EXPECT_VALID},
	{"features", "with_xmp.jxl", JXL_EXPECT_VALID},
	{"invalid", "empty.jxl", JXL_EXPECT_INVALID},
	{"invalid", "invalid-signature.jxl", JXL_EXPECT_INVALID},
	{"invalid", "truncated-header.jxl", JXL_EXPECT_INVALID},
	{"invalid", "truncated-payload.jxl", JXL_EXPECT_INVALID},
	{"photographic", "candle.jxl", JXL_EXPECT_VALID},
	{"photographic", "dice.jxl", JXL_EXPECT_VALID},
	{"photographic", "efb.jxl", JXL_EXPECT_VALID},
	{"photographic", "zoltan_tasi_unsplash.jxl", JXL_EXPECT_VALID},
};

int main(void) {
	int category_counts[5] = {0, 0, 0, 0, 0};
	static const char *category_names[] = {
		"conformance", "features", "photographic", "edge-cases", "invalid"
	};
	static const int expected_counts[] = {39, 128, 4, 13, 4};
	int total_valid = 0;
	int total_invalid = 0;
	size_t i;
	size_t category;

	gdSetErrorMethod(gdSilence);
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		for (category = 0;
			 category < sizeof(category_names) / sizeof(category_names[0]);
			 category++) {
			if (strcmp(cases[i].directory, category_names[category]) == 0) {
				category_counts[category]++;
				break;
			}
		}
		gdTestAssertMsg(category <
						sizeof(category_names) / sizeof(category_names[0]),
					"unknown JXL corpus category: %s\n", cases[i].directory);
		if (cases[i].expectation == JXL_EXPECT_VALID) {
			assert_valid_file(cases[i].directory, cases[i].filename);
			total_valid++;
		} else {
			assert_invalid_file(cases[i].directory, cases[i].filename);
			total_invalid++;
		}
	}
	gdClearErrorMethod();

	for (category = 0;
		 category < sizeof(category_names) / sizeof(category_names[0]);
		 category++) {
		gdTestAssertMsg(category_counts[category] == expected_counts[category],
						"JXL category %s has %d cases, expected %d\n",
						category_names[category], category_counts[category],
						expected_counts[category]);
	}
	gdTestAssertMsg(total_valid == 184,
					"JXL valid corpus contains %d cases, expected 184\n",
					total_valid);
	gdTestAssertMsg(total_invalid == 4,
					"JXL invalid corpus contains %d cases, expected 4\n",
					total_invalid);
	return gdNumFailures();
}
