#include "gd.h"
#include "gdtest.h"

int main(void)
{
	gdWebpReadOptions readOptions;
	gdWebpWriteOptions writeOptions;
	void (BGD_STDCALL *readOptionsInit)(gdWebpReadOptions *) = gdWebpReadOptionsInit;
	void (BGD_STDCALL *writeOptionsInit)(gdWebpWriteOptions *) = gdWebpWriteOptionsInit;
	int (BGD_STDCALL *isAnimated)(FILE *) = gdWebpIsAnimated;
	int (BGD_STDCALL *isAnimatedCtx)(gdIOCtxPtr) = gdWebpIsAnimatedCtx;
	int (BGD_STDCALL *isAnimatedPtr)(int, void *) = gdWebpIsAnimatedPtr;
	gdWebpReadPtr (BGD_STDCALL *readOpen)(FILE *, const gdWebpReadOptions *) =
		gdWebpReadOpen;
	gdWebpReadPtr (BGD_STDCALL *readOpenCtx)(gdIOCtxPtr, const gdWebpReadOptions *) =
		gdWebpReadOpenCtx;
	gdWebpReadPtr (BGD_STDCALL *readOpenPtr)(int, void *, const gdWebpReadOptions *) =
		gdWebpReadOpenPtr;
	int (BGD_STDCALL *readGetInfo)(gdWebpReadPtr, gdWebpInfo *) = gdWebpReadGetInfo;
	int (BGD_STDCALL *readNextFrame)(gdWebpReadPtr, gdWebpFrameInfo *, gdImagePtr *) =
		gdWebpReadNextFrame;
	int (BGD_STDCALL *readNextImage)(gdWebpReadPtr, gdWebpFrameInfo *, gdImagePtr *) =
		gdWebpReadNextImage;
	void (BGD_STDCALL *readClose)(gdWebpReadPtr) = gdWebpReadClose;
	gdWebpWritePtr (BGD_STDCALL *writeOpen)(FILE *, const gdWebpWriteOptions *) =
		gdWebpWriteOpen;
	gdWebpWritePtr (BGD_STDCALL *writeOpenCtx)(gdIOCtxPtr, const gdWebpWriteOptions *) =
		gdWebpWriteOpenCtx;
	gdWebpWritePtr (BGD_STDCALL *writeOpenPtr)(const gdWebpWriteOptions *) =
		gdWebpWriteOpenPtr;
	int (BGD_STDCALL *writeAddImage)(gdWebpWritePtr, gdImagePtr, int) =
		gdWebpWriteAddImage;
	void (BGD_STDCALL *writeClose)(gdWebpWritePtr) = gdWebpWriteClose;
	void *(BGD_STDCALL *writePtrFinish)(gdWebpWritePtr, int *) = gdWebpWritePtrFinish;

	gdWebpReadOptionsInit(&readOptions);
	gdWebpWriteOptionsInit(&writeOptions);

	gdTestAssert(readOptions.coalesced == 1);
	gdTestAssert(writeOptions.quality == -1);
	gdTestAssert(writeOptions.method == -1);

	gdTestAssert(readOptionsInit != NULL);
	gdTestAssert(writeOptionsInit != NULL);
	gdTestAssert(isAnimated != NULL);
	gdTestAssert(isAnimatedCtx != NULL);
	gdTestAssert(isAnimatedPtr != NULL);
	gdTestAssert(readOpen != NULL);
	gdTestAssert(readOpenCtx != NULL);
	gdTestAssert(readOpenPtr != NULL);
	gdTestAssert(readGetInfo != NULL);
	gdTestAssert(readNextFrame != NULL);
	gdTestAssert(readNextImage != NULL);
	gdTestAssert(readClose != NULL);
	gdTestAssert(writeOpen != NULL);
	gdTestAssert(writeOpenCtx != NULL);
	gdTestAssert(writeOpenPtr != NULL);
	gdTestAssert(writeAddImage != NULL);
	gdTestAssert(writeClose != NULL);
	gdTestAssert(writePtrFinish != NULL);

	return gdNumFailures();
}
