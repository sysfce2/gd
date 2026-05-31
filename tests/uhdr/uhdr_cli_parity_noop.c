#include "gd.h"
#include "gdtest.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#define MAX_METADATA_ENTRIES 32
#define MAX_METADATA_VALUES 3
#define METADATA_FLOAT_EPSILON 1e-6
#define RAW_MAX_DIFF 16
#define RAW_MAX_CHANGED_RATIO 0.005

#ifdef _WIN32
#define gdTestStrtok(s, sep, state) strtok_s((s), (sep), (state))
#else
#define gdTestStrtok(s, sep, state) strtok_r((s), (sep), (state))
#endif

typedef struct {
	char key[64];
	double values[MAX_METADATA_VALUES];
	int value_count;
} MetadataEntry;

typedef struct {
	MetadataEntry entries[MAX_METADATA_ENTRIES];
	int count;
} MetadataTable;

#ifndef _WIN32
static int has_single_quote(const char *s)
{
	const char *p;

	if (!s) {
		return 0;
	}

	for (p = s; *p != '\0'; p++) {
		if (*p == '\'') {
			return 1;
		}
	}

	return 0;
}
#endif

#ifdef _WIN32
static int has_double_quote(const char *s)
{
	const char *p;

	if (!s) {
		return 0;
	}

	for (p = s; *p != '\0'; p++) {
		if (*p == '"') {
			return 1;
		}
	}

	return 0;
}
#endif

static int cli_status_succeeded(int status, const char *cmd)
{
#ifdef _WIN32
	if (status != 0) {
		gdTestErrorMsg("CLI invocation failed: exit=%d, command=%s\n", status, cmd);
		return 0;
	}
#else
	if (WIFEXITED(status)) {
		int exit_code = WEXITSTATUS(status);
		if (exit_code != 0) {
			gdTestErrorMsg("CLI invocation failed: exit=%d, command=%s\n", exit_code, cmd);
			return 0;
		}
	} else {
		gdTestErrorMsg("CLI invocation did not exit normally: command=%s\n", cmd);
		return 0;
	}
#endif

	return 1;
}

static int run_cli_decode(const char *input_jpg, const char *metadata_path, const char *raw_path)
{
	char cmd[4096];
	int status;
	int rc;

	if (!input_jpg || !metadata_path || !raw_path) {
		gdTestErrorMsg("CLI invocation has NULL path argument\n");
		return 0;
	}

#ifdef _WIN32
	if (has_double_quote(input_jpg) || has_double_quote(metadata_path) || has_double_quote(raw_path)) {
		gdTestErrorMsg("CLI invocation paths must not include double quote characters\n");
		return 0;
	}

	rc = snprintf(cmd, sizeof(cmd),
		"ultrahdr_app -m 1 -j \"%s\" -f \"%s\" -z \"%s\" >nul 2>&1",
		input_jpg, metadata_path, raw_path);
#else
	if (has_single_quote(input_jpg) || has_single_quote(metadata_path) || has_single_quote(raw_path)) {
		gdTestErrorMsg("CLI invocation paths must not include single quote characters\n");
		return 0;
	}

	rc = snprintf(cmd, sizeof(cmd),
		"ultrahdr_app -m 1 -j '%s' -f '%s' -z '%s' >/dev/null 2>&1",
		input_jpg, metadata_path, raw_path);
#endif
	if (rc < 0 || (size_t) rc >= sizeof(cmd)) {
		gdTestErrorMsg("CLI command buffer overflow while preparing command\n");
		return 0;
	}

	status = system(cmd);
	if (status == -1) {
		gdTestErrorMsg("CLI invocation failed to spawn process: errno=%d\n", errno);
		return 0;
	}

	return cli_status_succeeded(status, cmd);
}

static int parse_metadata_file(const char *path, MetadataTable *table)
{
	FILE *fp;
	char line[512];

	if (!path || !table) {
		return 0;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		gdTestErrorMsg("failed to open metadata file: %s\n", path);
		return 0;
	}

	table->count = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		char *saveptr = NULL;
		char *token = gdTestStrtok(line, " \t\r\n", &saveptr);
		MetadataEntry *entry;

		if (!token || strncmp(token, "--", 2) != 0) {
			continue;
		}

		if (table->count >= MAX_METADATA_ENTRIES) {
			gdTestErrorMsg("too many metadata entries in %s\n", path);
			fclose(fp);
			return 0;
		}

		entry = &table->entries[table->count++];
		strncpy(entry->key, token, sizeof(entry->key) - 1);
		entry->key[sizeof(entry->key) - 1] = '\0';
		entry->value_count = 0;

		while ((token = gdTestStrtok(NULL, " \t\r\n", &saveptr)) != NULL) {
			char *end = NULL;
			double v;
			if (entry->value_count >= MAX_METADATA_VALUES) {
				gdTestErrorMsg("too many metadata values for key %s in %s\n", entry->key, path);
				fclose(fp);
				return 0;
			}
			v = strtod(token, &end);
			if (!end || *end != '\0') {
				gdTestErrorMsg("metadata parse failure for key %s in %s\n", entry->key, path);
				fclose(fp);
				return 0;
			}
			entry->values[entry->value_count++] = v;
		}
	}

	fclose(fp);
	return 1;
}

static const MetadataEntry *find_entry(const MetadataTable *table, const char *key)
{
	int i;

	for (i = 0; i < table->count; i++) {
		if (strcmp(table->entries[i].key, key) == 0) {
			return &table->entries[i];
		}
	}

	return NULL;
}

static int is_exact_key(const char *key)
{
	return strcmp(key, "--useBaseColorSpace") == 0;
}

static int compare_metadata_tables(const MetadataTable *lhs, const MetadataTable *rhs)
{
	int i;

	if (!lhs || !rhs) {
		return 0;
	}

	for (i = 0; i < lhs->count; i++) {
		int j;
		const MetadataEntry *a = &lhs->entries[i];
		const MetadataEntry *b = find_entry(rhs, a->key);

		if (!b) {
			gdTestErrorMsg("missing metadata key in GD output: %s\n", a->key);
			return 0;
		}

		if (a->value_count != b->value_count) {
			gdTestErrorMsg("metadata value count mismatch for key %s: %d vs %d\n",
				a->key, a->value_count, b->value_count);
			return 0;
		}

		for (j = 0; j < a->value_count; j++) {
			double av = a->values[j];
			double bv = b->values[j];
			double diff = fabs(av - bv);

			if (is_exact_key(a->key)) {
				if ((long) llround(av) != (long) llround(bv)) {
					gdTestErrorMsg("metadata exact mismatch for key %s[%d]: %.10f vs %.10f\n",
						a->key, j, av, bv);
					return 0;
				}
			} else if (diff > METADATA_FLOAT_EPSILON) {
				gdTestErrorMsg("metadata mismatch for key %s[%d]: %.10f vs %.10f (diff=%.10f)\n",
					a->key, j, av, bv, diff);
				return 0;
			}
		}
	}

	for (i = 0; i < rhs->count; i++) {
		if (!find_entry(lhs, rhs->entries[i].key)) {
			gdTestErrorMsg("unexpected metadata key in GD output: %s\n", rhs->entries[i].key);
			return 0;
		}
	}

	return 1;
}

static unsigned char *read_binary_file(const char *path, int *size)
{
	FILE *fp;
	long len;
	unsigned char *data;

	if (!path || !size) {
		return NULL;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		return NULL;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}

	len = ftell(fp);
	if (len <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return NULL;
	}

	data = (unsigned char *) malloc((size_t) len);
	if (!data) {
		fclose(fp);
		return NULL;
	}

	if (fread(data, 1, (size_t) len, fp) != (size_t) len) {
		free(data);
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	*size = (int) len;
	return data;
}

static int compare_raw_files(const char *lhs_path, const char *rhs_path)
{
	unsigned char *lhs = NULL;
	unsigned char *rhs = NULL;
	int lhs_size = 0;
	int rhs_size = 0;
	int i;
	int max_diff = 0;
	int changed = 0;
	double ratio;

	lhs = read_binary_file(lhs_path, &lhs_size);
	rhs = read_binary_file(rhs_path, &rhs_size);
	if (!lhs || !rhs) {
		gdTestErrorMsg("failed to read raw output for comparison\n");
		free(lhs);
		free(rhs);
		return 0;
	}

	if (lhs_size != rhs_size) {
		gdTestErrorMsg("raw output size mismatch: %d vs %d\n", lhs_size, rhs_size);
		free(lhs);
		free(rhs);
		return 0;
	}

	for (i = 0; i < lhs_size; i++) {
		int diff = abs((int) lhs[i] - (int) rhs[i]);
		if (diff > 0) {
			changed++;
		}
		if (diff > max_diff) {
			max_diff = diff;
		}
	}

	ratio = lhs_size > 0 ? (double) changed / (double) lhs_size : 0.0;
	if (max_diff > RAW_MAX_DIFF || ratio > RAW_MAX_CHANGED_RATIO) {
		gdTestErrorMsg("raw output mismatch: max_diff=%d changed=%d/%d ratio=%.8f\n",
			max_diff, changed, lhs_size, ratio);
		free(lhs);
		free(rhs);
		return 0;
	}

	free(lhs);
	free(rhs);
	return 1;
}

int main(void)
{
	char *sample_path = NULL;
	char *cli_src_meta = NULL;
	char *cli_src_raw = NULL;
	char *gd_out_jpg = NULL;
	char *cli_gd_meta = NULL;
	char *cli_gd_raw = NULL;
	MetadataTable src_meta;
	MetadataTable gd_meta;
	gdUhdrImagePtr src_im = NULL;
	gdUhdrImagePtr gd_im = NULL;
	gdUhdrError err;
	int rc;

	if (!gdTestAssertMsg(gdUhdrIsAvailable() == 1, "UltraHDR support should be enabled for this test\n")) {
		goto cleanup;
	}

	sample_path = gdTestFilePath("uhdr/uhdr_sample.jpg");
	if (!gdTestAssertMsg(sample_path != NULL, "failed to resolve UltraHDR sample path\n")) {
		goto cleanup;
	}

	cli_src_meta = gdTestTempFile("uhdr_cli_src_metadata.cfg");
	cli_src_raw = gdTestTempFile("uhdr_cli_src_dump.raw");
	gd_out_jpg = gdTestTempFile("uhdr_gd_noop_output.jpg");
	cli_gd_meta = gdTestTempFile("uhdr_cli_gd_metadata.cfg");
	cli_gd_raw = gdTestTempFile("uhdr_cli_gd_dump.raw");
	if (!gdTestAssertMsg(cli_src_meta && cli_src_raw && gd_out_jpg && cli_gd_meta && cli_gd_raw,
		"failed to allocate temp file paths\n")) {
		goto cleanup;
	}

	if (!gdTestAssertMsg(run_cli_decode(sample_path, cli_src_meta, cli_src_raw),
		"CLI decode for baseline image failed\n")) {
		goto cleanup;
	}

	memset(&err, 0, sizeof(err));
	src_im = gdUhdrImageCreateFromFile(sample_path, GD_UHDR_FORMAT_JPEG, &err);
	if (!gdTestAssertMsg(src_im != NULL,
		"gdUhdrImageCreateFromFile failed: code=%d provider=%d message=%s\n",
		err.code, err.provider_code, err.message)) {
		goto cleanup;
	}

	memset(&err, 0, sizeof(err));
	rc = gdUhdrImageFile(src_im, gd_out_jpg, GD_UHDR_FORMAT_JPEG, 95, &err);
	if (!gdTestAssertMsg(rc == GD_UHDR_SUCCESS,
		"gdUhdrImageFile failed: code=%d provider=%d message=%s\n",
		err.code, err.provider_code, err.message)) {
		goto cleanup;
	}

	if (!gdTestAssertMsg(run_cli_decode(gd_out_jpg, cli_gd_meta, cli_gd_raw),
		"CLI decode for GD output image failed\n")) {
		goto cleanup;
	}

	if (!gdTestAssertMsg(parse_metadata_file(cli_src_meta, &src_meta), "failed to parse baseline metadata\n")) {
		goto cleanup;
	}
	if (!gdTestAssertMsg(parse_metadata_file(cli_gd_meta, &gd_meta), "failed to parse GD metadata\n")) {
		goto cleanup;
	}
	if (!gdTestAssertMsg(compare_metadata_tables(&src_meta, &gd_meta),
		"metadata parity check failed\n")) {
		goto cleanup;
	}

	if (!gdTestAssertMsg(compare_raw_files(cli_src_raw, cli_gd_raw),
		"raw parity check failed\n")) {
		goto cleanup;
	}

	memset(&err, 0, sizeof(err));
	gd_im = gdUhdrImageCreateFromFile(gd_out_jpg, GD_UHDR_FORMAT_JPEG, &err);
	if (!gdTestAssertMsg(gd_im != NULL,
		"failed to reload GD output as UHDR: code=%d provider=%d message=%s\n",
		err.code, err.provider_code, err.message)) {
		goto cleanup;
	}

	gdTestAssertMsg(gdUhdrImageWidth(src_im) == gdUhdrImageWidth(gd_im),
		"output width mismatch: %d vs %d\n", gdUhdrImageWidth(src_im), gdUhdrImageWidth(gd_im));
	gdTestAssertMsg(gdUhdrImageHeight(src_im) == gdUhdrImageHeight(gd_im),
		"output height mismatch: %d vs %d\n", gdUhdrImageHeight(src_im), gdUhdrImageHeight(gd_im));
	gdTestAssertMsg(gdUhdrImageHasGainMap(src_im) == gdUhdrImageHasGainMap(gd_im),
		"gain map flag mismatch: %d vs %d\n", gdUhdrImageHasGainMap(src_im), gdUhdrImageHasGainMap(gd_im));

cleanup:
	if (gd_im) {
		gdUhdrImageDestroy(gd_im);
	}
	if (src_im) {
		gdUhdrImageDestroy(src_im);
	}
	gdFree(sample_path);
	gdFree(cli_src_meta);
	gdFree(cli_src_raw);
	gdFree(gd_out_jpg);
	gdFree(cli_gd_meta);
	gdFree(cli_gd_raw);

	return gdNumFailures();
}
