/**
 * DirectX 9 Output interceptor
 * Tom Gascoigne <tom@gascoigne.me>
 *
 * Basically, this dll works by loading itsself into
 * a directx program's address space, then obtaining 
 * pointers to the vtable for objects that it's interested
 * in. From here, the dll can locate the pointer to the
 * function it wants to hook, and replace it with
 * a pointer to its own function. This allows the
 * program to intercept any parameters and objects
 * in use by the function, where it can obtain video
 * output.
 * It then executes the original function and returns the
 * output, making the program unaware that anything happened.
 *
 */
#include "stdafx.h"

#include <d3d9.h>
#include <d3dx9.h>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

#include "MPEGEncoder.h"


#undef _HKDEBUG_

// Prototypes for d3d functions we want to hook

typedef HRESULT (WINAPI *CreateDevice_t)(IDirect3D9* Direct3D_Object, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, 
                    DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, 
                    IDirect3DDevice9** ppReturnedDeviceInterface);
typedef HRESULT (WINAPI *EndScene_t)(IDirect3DDevice9* surface);

// Function pointers to the original functions
CreateDevice_t D3DCreateDevice_orig;
EndScene_t D3DEndScene_orig;

// Our hooking functions
HRESULT WINAPI D3DCreateDevice_hook(IDirect3D9* Direct3D_Object, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, 
                    DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, 
                    IDirect3DDevice9** ppReturnedDeviceInterface);
HRESULT WINAPI D3DEndScene_hook(IDirect3DDevice9* device);

// vtable stuff
PDWORD IDirect3D9_vtable = NULL;

// Function indices: These are liable to change
// This is the functions index into the vtable
// Find these by reading through d3d9.h and counting in order
// IDirect3D9
#define CREATEDEVICE_VTI 16
// IDirect3DDevice9
#define ENDSCENE_VTI 42

MPEGEncoder* g_mpegEncoder = NULL;

HRESULT WINAPI HookCreateDevice();
DWORD WINAPI VTablePatchThread(LPVOID threadParam);


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	HMODULE d3dmodule;
	PBYTE funcAddress;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
//#ifdef _HKDEBUG_
		MessageBoxA(NULL, "DLL Injected", "DLL Injected", MB_ICONEXCLAMATION);
//#endif
		// First, We want to create our own D3D object so that we can hook CreateDevice
		if (HookCreateDevice() == D3D_OK)
		{
			return TRUE;
		} else {
#ifdef _HKDEBUG_
			MessageBoxA(NULL, "Unable to hook directx", "Unable to hook directx", MB_ICONEXCLAMATION);
#endif
			return FALSE;
		}

		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		/*
		// TODO: Hook directx shutdown for this
		if (g_mpegEncoder)
		{
			MessageBoxA(NULL, "Detached from process", "DLL", MB_ICONEXCLAMATION);
			delete g_mpegEncoder;
		}
		*/
		break;
	}
	return TRUE;
}

/**
 * This function sets up the hook for CreateDevice by replacing the pointer
 * to CreateDevice within IDirect3D9's vtable.
 */
HRESULT WINAPI HookCreateDevice()
{
	// Obtain a D3D object
	IDirect3D9* device = Direct3DCreate9(D3D_SDK_VERSION);
	if (!device)
	{
#ifdef _HKDEBUG_
		MessageBoxA(NULL, "Unable to create device", "CreateDevice", MB_ICONEXCLAMATION);
#endif
		return D3DERR_INVALIDCALL;
	}
	// Now we have an object, store a pointer to its vtable and release the object
	IDirect3D9_vtable = (DWORD*)*(DWORD*)device; // Confusing typecasts
	device->Release();

	// Unprotect the vtable for writing
	DWORD protectFlag;
	if (VirtualProtect(&IDirect3D9_vtable[CREATEDEVICE_VTI], sizeof(DWORD), PAGE_READWRITE, &protectFlag))
	{
		// Store the original CreateDevice pointer and shove our own function into the vtable
		*(DWORD*)&D3DCreateDevice_orig = IDirect3D9_vtable[CREATEDEVICE_VTI];
		*(DWORD*)&IDirect3D9_vtable[CREATEDEVICE_VTI] = (DWORD)D3DCreateDevice_hook;

		// Reprotect the vtable
		if (!VirtualProtect(&IDirect3D9_vtable[CREATEDEVICE_VTI], sizeof(DWORD), protectFlag, &protectFlag))
		{
#ifdef _HKDEBUG_
			MessageBoxA(NULL, "Unable to access vtable", "CreateDevice", MB_ICONEXCLAMATION);
#endif
			return D3DERR_INVALIDCALL;
		}
	} else {
#ifdef _HKDEBUG_
		MessageBoxA(NULL, "Unable to access vtable", "CreateDevice", MB_ICONEXCLAMATION);
#endif
		return D3DERR_INVALIDCALL;
	}
#ifdef _HKDEBUG_
	MessageBoxA(NULL, "Hooked CreateDevice call", "CreateDevice", MB_ICONEXCLAMATION);
#endif
	return D3D_OK;
}

/**
 * Thanks to HookCreateDevice(), The program should now call this method instead of
 * Direct3D's CreateDevice method. This allows us to then hook the IDirect3DDevice9 
 * methods we need
 */
HRESULT WINAPI D3DCreateDevice_hook(IDirect3D9* Direct3D_Object, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, 
                    DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, 
                    IDirect3DDevice9** ppReturnedDeviceInterface)
{
#ifdef _HKDEBUG_
	MessageBoxA(NULL, "CreateDevice called", "CreateDevice", MB_ICONEXCLAMATION);
#endif
	// Append the almighty D3DCREATE_MULTITHREADED flag...
	HRESULT result = D3DCreateDevice_orig(Direct3D_Object, Adapter, DeviceType, hFocusWindow, BehaviorFlags | D3DCREATE_MULTITHREADED, pPresentationParameters, ppReturnedDeviceInterface);

	// Now we've intercepted the program's call to CreateDevice and we have the IDirect3DDevice9 that it uses
	// We can get it's vtable and patch in our own detours
	// Reset the CreateDevice hook since it's no longer needed
	// Unprotect the vtable for writing
	DWORD protectFlag;
	if (VirtualProtect(&IDirect3D9_vtable[CREATEDEVICE_VTI], sizeof(DWORD), PAGE_READWRITE, &protectFlag))
	{
		// Store the original CreateDevice pointer and shove our own function into the vtable
		*(DWORD*)&IDirect3D9_vtable[CREATEDEVICE_VTI] = (DWORD)D3DCreateDevice_orig;

		// Reprotect the vtable
		if (!VirtualProtect(&IDirect3D9_vtable[CREATEDEVICE_VTI], sizeof(DWORD), protectFlag, &protectFlag))
		{
			return D3DERR_INVALIDCALL;
		}
	} else {
		return D3DERR_INVALIDCALL;
	}

	if (result == D3D_OK)
	{
		// Load the new vtable
		IDirect3D9_vtable = (DWORD*)*(DWORD*)*ppReturnedDeviceInterface;
#ifdef _HKDEBUG_
		MessageBoxA(NULL, "Loaded IDirect3DDevice9 vtable", "CreateDevice", MB_ICONEXCLAMATION);
#endif
		
		// Store pointers to the original functions that we want to hook
		*(PDWORD)&D3DEndScene_orig = (DWORD)IDirect3D9_vtable[ENDSCENE_VTI];

		if (!CreateThread(NULL, 0, VTablePatchThread, NULL, NULL, NULL))
		{
			return D3DERR_INVALIDCALL;
		}
	}

	return result;
}

/**
 * This is a thread which indefinately resets the vtable pointers to our own functions
 * This is needed because the program might set these pointers back to
 * their original values at any point
 */
DWORD WINAPI VTablePatchThread(LPVOID threadParam)
{
#ifdef _HKDEBUG_
	MessageBoxA(NULL, "VTable patch thread started", "Patch Thread", MB_ICONEXCLAMATION);
#endif
	while (true)
	{
		Sleep(100);

		*(DWORD*)&IDirect3D9_vtable[ENDSCENE_VTI] = (DWORD)D3DEndScene_hook;
	}
}

/**
 * This is called when each frame has finished
 * from here we can intercept the video output
 */
HRESULT WINAPI D3DEndScene_hook(IDirect3DDevice9* device)
{
	HRESULT result;
	if (!g_mpegEncoder)
	{
		g_mpegEncoder = new MPEGEncoder(device);
		result = D3DEndScene_orig(device);
		g_mpegEncoder->Start();
		return result;
	}
	result = D3DEndScene_orig(device);
#ifdef _HKDEBUG_
	MessageBoxA(NULL, "EndScene hook called", "EndScene", MB_ICONEXCLAMATION);
#endif
	// Here, we can get the output of the d3d device using GetBackBuffer, and send it off to
	// a file, or to be encoded, or whatever. *borat* Great success!!

	// capture now contains the screen buffer
	//D3DXSaveSurfaceToFile("Capture.bmp", D3DXIFF_BMP, capture, NULL, NULL);

	return result;
}