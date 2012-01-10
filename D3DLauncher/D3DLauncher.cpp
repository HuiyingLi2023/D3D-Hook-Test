// D3DLauncher.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "inject.h"

#pragma comment (lib, "detours.lib")

/**
 * Launches a specified program and injects the specified dll
 * Usage: D3DLauncher.exe D3DProgram.exe D3DHook.dll
 */
int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Usage: D3DLauncher.exe D3DProgram.exe D3DHook.dll" << std::endl;
		return 1;
	}
	std::string targetProgram;
	std::string targetDll;

	char currentDir_s[MAX_PATH];
	GetModuleFileNameA(0, currentDir_s, MAX_PATH);
	std::string currentDir(currentDir_s);
	currentDir = currentDir.substr(0, currentDir.find_last_of('\\'));
	std::cout << "Current dir: " << currentDir << std::endl;

	targetProgram = currentDir + std::string("\\") + argv[1];
	targetDll = currentDir + std::string("\\") + argv[2];

	std::cout << "Starting program " << targetProgram << " and injecting " << targetDll << std::endl;

	
	STARTUPINFOA startupInfo;
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo, 0, sizeof(STARTUPINFOA));
	memset(&processInfo, 0, sizeof(PROCESS_INFORMATION));
	startupInfo.cb = sizeof(STARTUPINFOA);

	// Spawn a process in SUSPENDED mode
	if (!CreateProcessA(NULL, (LPSTR)targetProgram.c_str(), NULL, NULL, false, CREATE_SUSPENDED, NULL, NULL, &startupInfo, &processInfo))
	{
		std::cerr << "CreateProcessA failed.." << std::endl;
		return 1;
	}
	// Get the id of the spawned process
	DWORD procId = processInfo.dwProcessId;
	if (!InjectDLL(procId, (char*)targetDll.c_str()))
	{
		std::cerr << "InjectDLL failed" << std::endl;
	}
	// Resume the process
	ResumeThread(processInfo.hThread);
	return 0;
}

