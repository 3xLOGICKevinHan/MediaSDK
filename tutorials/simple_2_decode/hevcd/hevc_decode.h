#pragma once

#include "common_utils.h"

class HEVCDecoder
{
public:
	HEVCDecoder();
	~HEVCDecoder();

	int Decode(LPBYTE lpbFrame, LONG nFrameSize);
	LPBYTE GetFrame(LPBITMAPINFO& lpbi);

private:
	int Init(LPBYTE lpbFrame, LONG nFrameSize);

private:
	void FillBITMAPINFO(mfxFrameSurface1* pSurface);
	void FillOutputBuffer(mfxFrameSurface1* pSurface);
	void WriteChunk(mfxU8* plane, mfxU16 factor, mfxU16 chunksize,
		mfxFrameInfo* pInfo, mfxFrameData* pData, mfxU32 i,
		mfxU32 j)
	{
		memcpy(m_lpbBufOut+ m_BufOutOffset, plane +
				(pInfo->CropY * pData->Pitch / factor + pInfo->CropX) +
				i * pData->Pitch + j, chunksize);
	}

private:
	MFXVideoSession m_session;
	std::unique_ptr<MFXVideoDECODE> m_mfxDEC;
	std::vector<mfxU8> m_surfaceBuffersData;
	std::vector<mfxFrameSurface1> m_pmfxSurfaces;

	BITMAPINFO m_biOutput{};
	LPBYTE m_lpbBufOut{};
	mfxU32  m_BufOutOffset{};

	bool m_initialized{false};
};
