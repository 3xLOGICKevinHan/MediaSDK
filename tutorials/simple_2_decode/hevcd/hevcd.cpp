#include "hevcd.h"
#include "hevc_decode.h"

void* create_hevcd()
{
	return new hevc_decoder();
}

void destroy_hevcd(void* p)
{
	delete (hevc_decoder*)p;
}

int	decode(void* p, unsigned char* lpbFrame, long nFrameSize)
{
	return ((hevc_decoder*)p)->decode(lpbFrame, nFrameSize);
}

unsigned char* GetFrame(void* p, void*& lpbi)
{
	return ((hevc_decoder*)p)->GetFrame((LPBITMAPINFO&)lpbi);
}