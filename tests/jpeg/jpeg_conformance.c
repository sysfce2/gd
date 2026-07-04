#include "gd.h"
#include "gdtest.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

#if defined(_WIN32)
#include "readdir.h"
#else
#include <dirent.h>
#endif

#include <sys/stat.h>

typedef enum {
	JPEG_EXPECT_VALID,
	JPEG_EXPECT_ROBUST,
	JPEG_EXPECT_INVALID
} jpeg_expectation;

typedef struct {
	const char *path;
	const char *directory;
	const char *subdirectory;
	int expected_count;
	jpeg_expectation expectation;
} corpus_category;

static int has_jpeg_suffix(const char *name) {
	size_t len = strlen(name);
	return (len > 4 &&
			(strcmp(name + len - 4, ".jpg") == 0 ||
			 strcmp(name + len - 4, ".JPG") == 0)) ||
		   (len > 5 &&
			(strcmp(name + len - 5, ".jpeg") == 0 ||
			 strcmp(name + len - 5, ".JPEG") == 0));
}

static int is_directory(const char *path) {
#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
	WIN32_FILE_ATTRIBUTE_DATA data;

	if (!GetFileAttributesEx(path, GetFileExInfoStandard, &data)) {
		return 0;
	}
	return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static FILE *open_corpus_file(const char *directory, const char *subdirectory,
						  const char *filename) {
	if (subdirectory == NULL) {
		return gdTestFileOpenX("jpeg", "conformance", directory, filename,
						   NULL);
	}
	return gdTestFileOpenX("jpeg", "conformance", directory, subdirectory,
					   filename, NULL);
}

static unsigned char *read_file(const char *directory,
								const char *subdirectory,
								const char *filename, int *size) {
	FILE *fp;
	long len;
	unsigned char *data;

	*size = 0;
	fp = open_corpus_file(directory, subdirectory, filename);
	if (fp == NULL) {
		return NULL;
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	len = ftell(fp);
	if (len < 0 || len > INT_MAX || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return NULL;
	}
	data = (unsigned char *)malloc(len > 0 ? (size_t)len : 1);
	if (data == NULL) {
		fclose(fp);
		return NULL;
	}
	if (len > 0 && fread(data, 1, (size_t)len, fp) != (size_t)len) {
		free(data);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	*size = (int)len;
	return data;
}

static void validate_image(gdImagePtr image, const char *path,
					   const char *decoder) {
	if (image == NULL) {
		return;
	}
	gdTestAssertMsg(gdImageSX(image) > 0 && gdImageSY(image) > 0,
					"%s returned invalid JPEG dimensions: %s\n", decoder,
					path);
	gdImageDestroy(image);
}

static int is_strict_invalid(const char *name) {
	static const char *const strict_files[] = {
		"empty.jpg", "missing-sof.jpg", "missing-sos.jpg",
		"null_height.jpg", "invalid-dimensions.jpg",
	};
	size_t i;

	for (i = 0; i < sizeof(strict_files) / sizeof(strict_files[0]); i++) {
		if (strcmp(name, strict_files[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

static void assert_file(const char *directory, const char *subdirectory,
					const char *name,
					jpeg_expectation expectation) {
	unsigned char *data;
	gdImagePtr file_image = NULL;
	gdImagePtr ptr_image;
	FILE *fp;
	int require_decode = expectation == JPEG_EXPECT_VALID;
	int require_reject = expectation == JPEG_EXPECT_INVALID;
	int size;
	char path[4096];

	if (snprintf(path, sizeof(path), "%s/%s%s%s", directory,
				 subdirectory != NULL ? subdirectory : "",
				 subdirectory != NULL ? "/" : "", name) >= (int)sizeof(path)) {
		gdTestErrorMsg("JPEG corpus path is too long: %s\n", name);
		return;
	}
	data = read_file(directory, subdirectory, name, &size);
	gdTestAssertMsg(data != NULL, "cannot read JPEG corpus file: %s\n", path);
	if (data == NULL) {
		return;
	}

	if (strcmp(name, "testorig12.jpg") == 0) {
#if BITS_IN_JSAMPLE == 12
		require_decode = 1;
#else
		require_decode = 0;
#endif
	}
	if (expectation == JPEG_EXPECT_ROBUST && is_strict_invalid(name)) {
		require_reject = 1;
	}

	fp = open_corpus_file(directory, subdirectory, name);
	gdTestAssertMsg(fp != NULL, "cannot reopen JPEG corpus file: %s\n", path);
	if (fp != NULL) {
		file_image = gdImageCreateFromJpeg(fp);
		fclose(fp);
	}
	ptr_image = gdImageCreateFromJpegPtrEx(size, data, 1);

	if (require_decode) {
		gdTestAssertMsg(file_image != NULL,
						"valid JPEG failed file decoding: %s\n", path);
		gdTestAssertMsg(ptr_image != NULL,
						"valid JPEG failed pointer decoding: %s\n", path);
	} else if (require_reject) {
		gdTestAssertMsg(file_image == NULL,
						"strict-invalid JPEG decoded from file: %s\n", path);
		gdTestAssertMsg(ptr_image == NULL,
						"strict-invalid JPEG decoded from pointer: %s\n", path);
	}

	validate_image(file_image, path, "file decoder");
	validate_image(ptr_image, path, "pointer decoder");
	free(data);
}

static int scan_directory(const char *path, const char *directory,
						  const char *subdirectory,
						  jpeg_expectation expectation) {
	DIR *handle;
	struct dirent *entry;
	int files = 0;

	handle = opendir(path);
	if (handle == NULL) {
		gdTestErrorMsg("cannot open JPEG conformance directory: %s\n",
					   path);
		return 0;
	}

	while ((entry = readdir(handle)) != NULL) {
		char entry_path[4096];

		if (strcmp(entry->d_name, ".") == 0 ||
			strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		if (snprintf(entry_path, sizeof(entry_path), "%s/%s", path,
					 entry->d_name) >= (int)sizeof(entry_path)) {
			gdTestErrorMsg("JPEG corpus path is too long: %s/%s\n", path,
						   entry->d_name);
			continue;
		}
		if (is_directory(entry_path)) {
			continue;
		}
		if (!has_jpeg_suffix(entry->d_name)) {
			continue;
		}
		files++;
		assert_file(directory, subdirectory, entry->d_name, expectation);
	}
	closedir(handle);
	return files;
}

int main(void) {
	static const corpus_category categories[] = {
		{"valid", "valid", NULL, 41, JPEG_EXPECT_VALID},
		{"invalid", "invalid", NULL, 116, JPEG_EXPECT_ROBUST},
		{"non-conformant/truncated", "non-conformant", "truncated", 12,
		 JPEG_EXPECT_ROBUST},
		{"non-conformant/extraneous-data", "non-conformant",
		 "extraneous-data", 1, JPEG_EXPECT_ROBUST},
		{"non-conformant/marker-quirks", "non-conformant", "marker-quirks",
		 1, JPEG_EXPECT_ROBUST},
		{"non-conformant/metadata-quirks", "non-conformant",
		 "metadata-quirks", 5, JPEG_EXPECT_ROBUST},
		{"non-conformant/progressive-quirks", "non-conformant",
		 "progressive-quirks", 1, JPEG_EXPECT_ROBUST},
		{"crash-repro/jpeg-decoder", "crash-repro", "jpeg-decoder", 15,
		 JPEG_EXPECT_ROBUST},
		{"crash-repro/jpeg-decoder-257", "crash-repro", "jpeg-decoder-257",
		 27, JPEG_EXPECT_ROBUST},
		{"crash-repro/libjpeg-turbo", "crash-repro", "libjpeg-turbo", 6,
		 JPEG_EXPECT_ROBUST},
		{"crash-repro/zune-jpeg", "crash-repro", "zune-jpeg", 29,
		 JPEG_EXPECT_ROBUST},
	};
	char *root = gdTestFilePathX("jpeg", "conformance", NULL);
	int total = 0;
	size_t i;

	gdSetErrorMethod(gdSilence);
	for (i = 0; i < sizeof(categories) / sizeof(categories[0]); i++) {
		char directory[4096];
		int count;

		if (snprintf(directory, sizeof(directory), "%s/%s", root,
					 categories[i].path) >= (int)sizeof(directory)) {
			gdTestErrorMsg("JPEG corpus path is too long: %s/%s\n", root,
						   categories[i].path);
			continue;
		}
		count = scan_directory(directory, categories[i].directory,
						   categories[i].subdirectory,
						   categories[i].expectation);
		gdTestAssertMsg(count == categories[i].expected_count,
						"JPEG category %s contains %d images, expected %d\n",
						categories[i].path, count, categories[i].expected_count);
		total += count;
	}
	gdClearErrorMethod();

	gdTestAssertMsg(total == 254,
					"JPEG corpus contains %d images, expected 254\n", total);
	free(root);
	return gdNumFailures();
}
