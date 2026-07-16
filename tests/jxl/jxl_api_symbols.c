#include "gd.h"
#include "gdtest.h"

int main(void) {
	gdJxlReadOptions read_options;
	gdJxlWriteOptions write_options;
	gdJxlReadPtr (*read_open)(FILE *, const gdJxlReadOptions *);
	gdJxlReadPtr (*read_open_ctx)(gdIOCtxPtr, const gdJxlReadOptions *);
	gdJxlReadPtr (*read_open_ptr)(int, void *, const gdJxlReadOptions *);
	int (*read_get_info)(gdJxlReadPtr, gdJxlInfo *);
	int (*read_next_image)(gdJxlReadPtr, int *, gdImagePtr *);
	int (*read_next_frame)(gdJxlReadPtr, gdJxlFrameInfo *, gdImagePtr *);
	void (*read_close)(gdJxlReadPtr);
	gdJxlWritePtr (*write_open)(FILE *, const gdJxlWriteOptions *);
	gdJxlWritePtr (*write_open_ctx)(gdIOCtxPtr, const gdJxlWriteOptions *);
	gdJxlWritePtr (*write_open_ptr)(const gdJxlWriteOptions *);
	int (*write_add_image)(gdJxlWritePtr, gdImagePtr, int);
	void (*write_close)(gdJxlWritePtr);
	void *(*write_ptr_finish)(gdJxlWritePtr, int *);

	gdJxlReadOptionsInit(&read_options);
	gdJxlWriteOptionsInit(&write_options);

	gdTestAssert(read_options.struct_size == sizeof(read_options));
	gdTestAssert(read_options.coalesced == 1);
	gdTestAssert(write_options.struct_size == sizeof(write_options));
	gdTestAssert(write_options.canvasWidth == 0);
	gdTestAssert(write_options.canvasHeight == 0);
	gdTestAssert(write_options.distance == 1.0f);
	gdTestAssert(write_options.effort == 7);
	gdTestAssert(write_options.loopCount == 0);

	read_open = gdJxlReadOpen;
	read_open_ctx = gdJxlReadOpenCtx;
	read_open_ptr = gdJxlReadOpenPtr;
	read_get_info = gdJxlReadGetInfo;
	read_next_image = gdJxlReadNextImage;
	read_next_frame = gdJxlReadNextFrame;
	read_close = gdJxlReadClose;
	write_open = gdJxlWriteOpen;
	write_open_ctx = gdJxlWriteOpenCtx;
	write_open_ptr = gdJxlWriteOpenPtr;
	write_add_image = gdJxlWriteAddImage;
	write_close = gdJxlWriteClose;
	write_ptr_finish = gdJxlWritePtrFinish;

	gdTestAssert(read_open != NULL);
	gdTestAssert(read_open_ctx != NULL);
	gdTestAssert(read_open_ptr != NULL);
	gdTestAssert(read_get_info != NULL);
	gdTestAssert(read_next_image != NULL);
	gdTestAssert(read_next_frame != NULL);
	gdTestAssert(read_close != NULL);
	gdTestAssert(write_open != NULL);
	gdTestAssert(write_open_ctx != NULL);
	gdTestAssert(write_open_ptr != NULL);
	gdTestAssert(write_add_image != NULL);
	gdTestAssert(write_close != NULL);
	gdTestAssert(write_ptr_finish != NULL);

	return gdNumFailures();
}
