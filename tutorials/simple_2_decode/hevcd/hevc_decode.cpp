// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "hevc_decode.h"

hevc_decoder::hevc_decoder()
{
}

hevc_decoder::~hevc_decoder()
{
    // ===================================================================
    // Clean up resources
    //  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
    //    some surfaces may still be locked by internal Media SDK resources.

    if (m_mfxDEC.get()) m_mfxDEC->Close();
    // session closed automatically on destruction

    Release();
    if (m_lpbBufOut) free(m_lpbBufOut);
}

int hevc_decoder::initialize(LPBYTE lpbFrame, LONG nFrameSize)
{
    mfxStatus sts = MFX_ERR_NONE;

    // Initialize Intel Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)
    // - Version 1.0 is selected for greatest backwards compatibility.
    // OS specific notes
    // - On Windows both SW and HW libraries may present
    // - On Linux only HW library only is available
    //   If more recent API features are needed, change the version accordingly
    mfxIMPL impl = MFX_IMPL_SOFTWARE;
    mfxVersion ver = { {0, 1} };

    sts = Initialize(impl, ver, &m_session, NULL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Create Media SDK decoder
    m_mfxDEC = std::make_unique<MFXVideoDECODE>(m_session);

    // Set required video parameters for decode
    mfxVideoParam mfxVideoParams;
    memset(&mfxVideoParams, 0, sizeof(mfxVideoParams));
    mfxVideoParams.mfx.CodecId = MFX_CODEC_HEVC;
    mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    // 4. Load the HEVC plugin
    mfxPluginUID codecUID;
    bool success = true;
    codecUID = msdkGetPluginUID(impl, MSDK_VDECODE, mfxVideoParams.mfx.CodecId);

    if (AreGuidsEqual(codecUID, MSDK_PLUGINGUID_NULL)) {
        printf("Get Plugin UID for HEVC is failed.\n");
        success = false;
    }

    printf("Loading HEVC plugin: %s\n", ConvertGuidToString(codecUID));

    //On the success of get the UID, load the plugin
    if (success) {
        sts = MFXVideoUSER_Load(m_session, &codecUID, ver.Major);
        if (sts < MFX_ERR_NONE) {
            printf("Loading HEVC plugin failed\n");
            success = false;
        }
    }

    // Prepare Media SDK bit stream buffer
    // - Arbitrary buffer size for this example
    mfxBitstream mfxBS;
    memset(&mfxBS, 0, sizeof(mfxBS));
    mfxBS.Data = lpbFrame;
    mfxBS.DataOffset = 0;
    mfxBS.MaxLength = mfxBS.DataLength = nFrameSize;

    sts = m_mfxDEC->DecodeHeader(&mfxBS, &mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Validate video decode parameters (optional)
    sts = m_mfxDEC->Query(&mfxVideoParams, &mfxVideoParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Query number of required surfaces for decoder
    mfxFrameAllocRequest Request;
    memset(&Request, 0, sizeof(Request));
    sts = m_mfxDEC->QueryIOSurf(&mfxVideoParams, &Request);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxU16 numSurfaces = Request.NumFrameSuggested;

    // Allocate surfaces for decoder
    // - Width and height of buffer must be aligned, a multiple of 32
    // - Frame surface array keeps pointers all surface planes and general frame info
    mfxU16 width = (mfxU16)MSDK_ALIGN32(Request.Info.Width);
    mfxU16 height = (mfxU16)MSDK_ALIGN32(Request.Info.Height);
    mfxU8 bitsPerPixel = 12;        // NV12 format is a 12 bits per pixel format
    mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
    m_surfaceBuffersData.resize(surfaceSize * numSurfaces);
    mfxU8* surfaceBuffers = m_surfaceBuffersData.data();

    // Allocate surface headers (mfxFrameSurface1) for decoder
    m_pmfxSurfaces.resize(numSurfaces);
    for (int i = 0; i < numSurfaces; i++) {
        memset(&m_pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
        m_pmfxSurfaces[i].Info = mfxVideoParams.mfx.FrameInfo;
        m_pmfxSurfaces[i].Data.Y = &surfaceBuffers[surfaceSize * i];
        m_pmfxSurfaces[i].Data.U = m_pmfxSurfaces[i].Data.Y + width * height;
        m_pmfxSurfaces[i].Data.V = m_pmfxSurfaces[i].Data.U + 1;
        m_pmfxSurfaces[i].Data.Pitch = width;
    }

    // Initialize the Media SDK decoder
    sts = m_mfxDEC->Init(&mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

int hevc_decoder::decode(LPBYTE lpbFrame, LONG nFrameSize)
{
    if (!m_initialized)
    {
        m_initialized = initialize(lpbFrame, nFrameSize) == MFX_ERR_NONE;
        if (!m_initialized) return -1;
    }

    // ===============================================================
    // Start decoding the frames
    //
    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint syncp;
    mfxFrameSurface1* pmfxOutSurface = NULL;
    int nIndex = 0;

    // Prepare Media SDK bit stream buffer
    // - Arbitrary buffer size for this example
    mfxBitstream mfxBS;
    memset(&mfxBS, 0, sizeof(mfxBS));
    mfxBS.Data = lpbFrame;
    mfxBS.DataOffset = 0;
    mfxBS.MaxLength = mfxBS.DataLength = nFrameSize;

    //
    // Stage 1: Main decoding loop
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

        if (MFX_ERR_MORE_DATA == sts) {
            MSDK_BREAK_ON_ERROR(sts);
        }

        if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
            nIndex = GetFreeSurfaceIndex(m_pmfxSurfaces);        // Find free frame surface
            MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);
        }
        // Decode a frame asychronously (returns immediately)
        //  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
        sts = m_mfxDEC->DecodeFrameAsync(&mfxBS, &m_pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);

        // Ignore warnings if output is available,
        // if no output and no action required just repeat the DecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncp)
            sts = MFX_ERR_NONE;

        if (MFX_ERR_NONE == sts)
            sts = m_session.SyncOperation(syncp, 60000);      // Synchronize. Wait until decoded frame is ready

        if (MFX_ERR_NONE == sts) {
            FillBITMAPINFO(pmfxOutSurface);
            FillOutputBuffer(pmfxOutSurface);
            return sts;
        }
    }

    // MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 2: Retrieve the buffered decoded frames
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

        nIndex = GetFreeSurfaceIndex(m_pmfxSurfaces);        // Find free frame surface
        MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);

        // Decode a frame asychronously (returns immediately)
        sts = m_mfxDEC->DecodeFrameAsync(NULL, &m_pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);

        // Ignore warnings if output is available,
        // if no output and no action required just repeat the DecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncp)
            sts = MFX_ERR_NONE;

        if (MFX_ERR_NONE == sts)
            sts = m_session.SyncOperation(syncp, 60000);      // Synchronize. Waits until decoded frame is ready

        if (MFX_ERR_NONE == sts) {
            FillBITMAPINFO(pmfxOutSurface);
            FillOutputBuffer(pmfxOutSurface);
            break;
        }
    }

    // MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

void hevc_decoder::FillBITMAPINFO(mfxFrameSurface1* pSurface)
{
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxU16 h, w;

    if (pInfo->CropH > 0 && pInfo->CropW > 0)
    {
        w = pInfo->CropW;
        h = pInfo->CropH;
    }
    else
    {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    m_biOutput.bmiHeader.biWidth = w;
    m_biOutput.bmiHeader.biHeight = h;
    m_biOutput.bmiHeader.biSizeImage = w * h * 3 / 2;
}

void hevc_decoder::FillOutputBuffer(mfxFrameSurface1* pSurface)
{
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;
    mfxU16 i, j, h, w;
    mfxStatus sts = MFX_ERR_NONE;

    if (pInfo->CropH > 0 && pInfo->CropW > 0)
    {
        w = pInfo->CropW;
        h = pInfo->CropH;
    }
    else
    {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    m_lpbBufOut = (LPBYTE)realloc(nullptr, w * h * 3 / 2);

    if (pInfo->FourCC != MFX_FOURCC_RGB4 && pInfo->FourCC != MFX_FOURCC_A2RGB10)
    {
        m_BufOutOffset = {};

        for (i = 0; i < pInfo->CropH; i++)
        {
            WriteChunk(pData->Y, 1, pInfo->CropW, pInfo, pData, i, 0);
            m_BufOutOffset += pInfo->CropW;
        }

        h = pInfo->CropH / 2;
        w = pInfo->CropW;
        for (i = 0; i < h; i++)
            for (j = 1; j < w; j += 2)
            {
                WriteChunk(pData->UV, 2, 1, pInfo, pData, i, j);
                ++m_BufOutOffset;
            }
        for (i = 0; i < h; i++)
            for (j = 0; j < w; j += 2)
            {
                WriteChunk(pData->UV, 2, 1, pInfo, pData, i, j);
                ++m_BufOutOffset;
            }
    }
}

LPBYTE hevc_decoder::GetFrame(LPBITMAPINFO& lpbi)
{
    lpbi = &m_biOutput;
    return m_lpbBufOut;
}
