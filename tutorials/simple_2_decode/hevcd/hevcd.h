#pragma once

// exported functions
extern "C"
{
	void* create_hevcd();
	void  destroy_hevcd(void* p);

	int	decode(void* p, unsigned char* lpbFrame, long nFrameSize);
	unsigned char* GetFrame(void* p, void*& lpbi);
}