// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include <d3d9.h>
#include <d3dx9.h>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

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
HRESULT WINAPI D3DEndScene_hook(IDirect3DDevice9* surface);

// vtable stuff
PDWORD IDirect3D9_vtable = NULL;
// Function indices: These are liable to change
// This is the functions index into the vtable
// Find these by reading through d3d9.h and counting in order

// IDirect3D9
#define CREATEDEVICE_VTI 16
// IDirect3DDevice9
#define ENDSCENE_VTI 42

HRESULT WINAPI HookCreateDevice();


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
		MessageBoxA(NULL, "DLL Injected", "DLL Injected", MB_ICONEXCLAMATION);
		// First, We want to create our own D3D object so that we can hook CreateDevice
		if (HookCreateDevice() == D3D_OK)
		{
			return TRUE;
		} else {
			MessageBoxA(NULL, "Unable to hook directx", "Unable to hook directx", MB_ICONEXCLAMATION);
			return FALSE;
		}

		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

HRESULT WINAPI HookCreateDevice()
{
	// Obtain a D3D object
	IDirect3D9* device = Direct3DCreate9(D3D_SDK_VERSION);
	if (!device)
	{
		MessageBoxA(NULL, "Unable to create device", "CreateDevice", MB_ICONEXCLAMATION);
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
			MessageBoxA(NULL, "Unable to access vtable", "CreateDevice", MB_ICONEXCLAMATION);
			return D3DERR_INVALIDCALL;
		}
	} else {
		MessageBoxA(NULL, "Unable to access vtable", "CreateDevice", MB_ICONEXCLAMATION);
		return D3DERR_INVALIDCALL;
	}
	MessageBoxA(NULL, "Hooked CreateDevice call", "CreateDevice", MB_ICONEXCLAMATION);
	return D3D_OK;
}

/**
 * This is a thread which indefinately resets the vtable pointers to our own functions
 * This is needed because the program might set these pointers back to
 * their original values at any point
 */
DWORD WINAPI VTablePatchThread(LPVOID threadParam)
{
	MessageBoxA(NULL, "VTable patch thread started", "Patch Thread", MB_ICONEXCLAMATION);
	while (true)
	{
		Sleep(100);

		*(DWORD*)&IDirect3D9_vtable[ENDSCENE_VTI] = (DWORD)D3DEndScene_hook;
	}
}

HRESULT WINAPI D3DCreateDevice_hook(IDirect3D9* Direct3D_Object, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, 
                    DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, 
                    IDirect3DDevice9** ppReturnedDeviceInterface)
{
	MessageBoxA(NULL, "CreateDevice called", "CreateDevice", MB_ICONEXCLAMATION);
	HRESULT result = D3DCreateDevice_orig(Direct3D_Object, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
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
		MessageBoxA(NULL, "Loaded IDirect3DDevice9 vtable", "CreateDevice", MB_ICONEXCLAMATION);
		
		// Store pointers to the original functions that we want to hook
		*(PDWORD)&D3DEndScene_orig = (DWORD)IDirect3D9_vtable[ENDSCENE_VTI];

		if (!CreateThread(NULL, 0, VTablePatchThread, NULL, NULL, NULL))
		{
			return D3DERR_INVALIDCALL;
		}
	}

	return result;
}

HRESULT WINAPI D3DEndScene_hook(IDirect3DDevice9* surface)
{
	HRESULT result = D3DEndScene_orig(surface);
	MessageBoxA(NULL, "EndScene hook called", "EndScene hook called", MB_ICONEXCLAMATION);

	return result;
}