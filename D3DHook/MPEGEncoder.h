#pragma once

#include <d3d9.h>
#include <d3dx9.h>

#undef _HKDEBUG_

class MPEGEncoder
{
private:
	static MPEGEncoder* m_singleton;
	IDirect3DDevice9* m_captureDevice;
	bool m_running;
	
public:
	MPEGEncoder(IDirect3DDevice9* captureDevice);
	virtual ~MPEGEncoder();

	// Thread which locks captureSurface and encodes to mpeg
	void MPEGEncode();
	void Start();
	void Stop();


	static MPEGEncoder* Get()
	{
		if (!m_singleton)
		{
			throw std::exception("Accessed uninitialized singleton");
		}
		return m_singleton;
	}
};

/**
 * Since we cannot specify a C++ function to CreateThread,
 * here is a C wrapper function which will call the appropriate
 * C++ function.
 */
DWORD WINAPI StartEncodingThread(LPVOID lpParam);