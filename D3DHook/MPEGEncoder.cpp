#include "stdafx.h"

#include "MPEGEncoder.h"

#include <sstream>

MPEGEncoder* MPEGEncoder::m_singleton = NULL;

DWORD WINAPI StartEncodingThread(LPVOID lpParam)
{
	MPEGEncoder::Get()->MPEGEncode();
	return 0;
}

MPEGEncoder::MPEGEncoder(IDirect3DDevice9* captureDevice) :
				m_captureDevice(captureDevice)
{
	if (m_singleton)
	{
		throw std::exception("Singleton already initialized");
	} else {
#ifdef _HKDEBUG_
		MessageBoxA(NULL, "Created singleton", "MPEGEncoder", MB_ICONEXCLAMATION);
#endif
		m_singleton = this;
		m_renderMutex = CreateMutex(NULL, false, NULL);
	}

}
MPEGEncoder::~MPEGEncoder()
{

}

void MPEGEncoder::Start()
{
	m_running = true;
	CreateThread(NULL, 0, StartEncodingThread, NULL, 0, NULL);
}

void MPEGEncoder::Stop()
{
	m_running = false;
}

void MPEGEncoder::MPEGEncode()
{
	
#ifdef _HKDEBUG_
	MessageBoxA(NULL, "Started encoding thread", "MPEGEncoder", MB_ICONEXCLAMATION);
#endif
	HRESULT tmpResult = S_OK;
	IDirect3DSurface9* capture = NULL;
	IDirect3DSurface9* rendertarget = NULL;
	D3DSURFACE_DESC surfaceDesc;

	tmpResult = m_captureDevice->GetRenderTarget(0, &rendertarget);
	tmpResult = rendertarget->GetDesc(&surfaceDesc);
	tmpResult = m_captureDevice->CreateOffscreenPlainSurface(surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format, D3DPOOL_SYSTEMMEM, &capture, NULL);
	if (tmpResult != S_OK)
	{
#ifdef _HKDEBUG_
		MessageBoxA(NULL, "Couldn't get front buffer data", "EndScene", MB_ICONEXCLAMATION);
#endif
	}
	int i = 0;
	while (m_running)
	{
		Sleep(1000/30);
		// Store the output in an IDirect3DSurface9
		tmpResult = m_captureDevice->GetRenderTargetData(rendertarget, capture);
		if (tmpResult != S_OK)
		{
			// We couldn't capture the screen, give up this frame..
#ifdef _HKDEBUG_
			MessageBoxA(NULL, "Couldn't get front buffer data", "MPEGEncode", MB_ICONEXCLAMATION);
#endif
		}
		D3DXSaveSurfaceToFile("Capture.bmp", D3DXIFF_BMP, capture, NULL, NULL);
	}
	capture->Release();
}