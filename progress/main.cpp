// 
// forked from CUE SDK Example progress.cpp
//

#include "CUESDK.h"

#include <iostream>
#include <algorithm>
#include <thread>
#include <future>
#include <vector>
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <psapi.h>

const char* toString(CorsairError error) 
{
	switch (error) {
	case CE_Success : 
		return "CE_Success";
	case CE_ServerNotFound:
		return "CE_ServerNotFound";
	case CE_NoControl:
		return "CE_NoControl";
	case CE_ProtocolHandshakeMissing:
		return "CE_ProtocolHandshakeMissing";
	case CE_IncompatibleProtocol:
		return "CE_IncompatibleProtocol";
	case CE_InvalidArguments:
		return "CE_InvalidArguments";
	default:
		return "unknown error";
	}
}

const int redZone = 80; // darkOrangeStop
const int yellowStop = 60;
const int lightOrangeStop = 70;

double getKeyboardWidth(CorsairLedPositions *ledPositions)
{
	const auto minmaxLeds = std::minmax_element(ledPositions->pLedPosition, ledPositions->pLedPosition + ledPositions->numberOfLed,
		[](const CorsairLedPosition &clp1, const CorsairLedPosition &clp2) {
		return clp1.left < clp2.left;
	});
	return minmaxLeds.second->left + minmaxLeds.second->width - minmaxLeds.first->left;
}

DWORD getProcessId() {
	HWND hWnd = FindWindowA(NULL, "Rocket League (32-bit, DX9)");

	if (hWnd == NULL) {
		return NULL;
	}

	DWORD procId;
	GetWindowThreadProcessId(hWnd, &procId);

	if (procId == NULL) {
		return NULL;
	}

	return procId;
}

HANDLE initProcessHandle() 
{
	DWORD procId = getProcessId();

	return OpenProcess(PROCESS_VM_READ, true, procId);
}

DWORD_PTR GetProcessBaseAddress()
{
	DWORD processID = getProcessId();
	DWORD_PTR   baseAddress = 0;
	HANDLE      processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
	HMODULE     *moduleArray;
	LPBYTE      moduleArrayBytes;
	DWORD       bytesRequired;

	if (processHandle)
	{
		if (EnumProcessModules(processHandle, NULL, 0, &bytesRequired))
		{
			if (bytesRequired)
			{
				moduleArrayBytes = (LPBYTE)LocalAlloc(LPTR, bytesRequired);

				if (moduleArrayBytes)
				{
					unsigned int moduleCount;

					moduleCount = bytesRequired / sizeof(HMODULE);
					moduleArray = (HMODULE *)moduleArrayBytes;

					if (EnumProcessModules(processHandle, moduleArray, bytesRequired, &bytesRequired))
					{
						baseAddress = (DWORD_PTR)moduleArray[0];
					}

					LocalFree(moduleArrayBytes);
				}
			}
		}

		CloseHandle(processHandle);
	}

	return baseAddress;
}

int getRocketLeagueTurbo(HANDLE h, INT_PTR p) 
{
	if (h == NULL || p == 0) {
		// handler isn't initalized,
		// rocket league probs isn't open.
		return 0;
	}
	
	int value;

	//std::cout << "\n getting " << std::hex << p;

	ReadProcessMemory(h, (LPVOID)p, &value, 4, NULL);

	//std::cout << "\n got something!\n-> " << value << "\n";
	
	return value;
}

int getRocketLeagueTurboPointer(HANDLE h) {

	if (h == NULL) {
		// handler isn't initalized,
		// rocket league probs isn't open.
		return 0;
	}
	//+015817E0+120+50+6f4+21c
	std::cout << std::endl << std::hex << (INT_PTR)(GetProcessBaseAddress()) << std::endl;
	INT_PTR base = (INT_PTR)(GetProcessBaseAddress()) + 0x0062465C;

	int offsets[6] = { 0x4A0, 0x54, 0x4C, 0x520, 0x54 };

	int addr = base;

	int b;
	ReadProcessMemory(h, (LPVOID)(base), &b, 4, 0);
	addr = b;
	//cout

	for (int i = 0; i < 7; ++i) {
		int b;
		ReadProcessMemory(h, (LPVOID)(addr + offsets[i]), &b, 4, 0);
		addr = addr + b;
		std::cout << i << " = " << std::hex << b << "+" << std::hex << offsets[i] << " so addr is now " << std::hex << addr << std::endl;
	}

	return addr;
}

/*
int lookupRocketLeagueMemoryLocation(HANDLE h) {
	// consecutive postfixes are 4 8 C.
	// i guess we can search for every location % 0x10 == 0, and make sure each postfix+it are both the same and == 0-100
	// if we can't find it, return 0.
	// if we can, return the 4 prefixed location.
	// when they're released (e.g. match ended,) reading code should note the value being greater than 100, and recall me.

}
*/
int main()
{
	HANDLE handle = NULL;
	UINT_PTR pointer = 0;
	bool cantfind = false;

	CorsairPerformProtocolHandshake();
	if (const auto error = CorsairGetLastError()) {
		std::cout << "Handshake failed: " << toString(error) << std::endl;
		getchar();
		return -1;
	}

	const auto ledPositions = CorsairGetLedPositions();
	if (ledPositions && ledPositions->numberOfLed > 0) {
				
		const auto keyboardWidth = getKeyboardWidth(ledPositions);
		const auto numberOfSteps = 100;
		std::cout << "Working... Press Escape to close program..." << std::endl << std::endl;
		for (auto n = 0; !GetAsyncKeyState(VK_ESCAPE); n++) {
			
			if (handle == NULL) {
				handle = initProcessHandle();
				pointer = getRocketLeagueTurboPointer(handle);
				if (handle == NULL && cantfind == false) {
					std::cout << "Can't find Rocket League. Please start the gaaaaame!" << std::endl;
					cantfind = true;
				}
			}

			if (cantfind && handle != NULL) {
				std::cout << "Found Rocket League!" << std::endl;
				cantfind = false;
			}

			// XXX: Get this value repeatably. I suck at it.
			// For demos, I used cheat engine to grab the memory locations.
			auto percentage = getRocketLeagueTurbo(handle, 0x32E1FC8C);

			std::vector<CorsairLedColor> vec;
			//const auto currWidth = double(keyboardWidth) * (n % (numberOfSteps + 1)) / numberOfSteps;
			const auto currWidth = double(keyboardWidth) * percentage / numberOfSteps;

			for (auto i = 0; i < ledPositions->numberOfLed; i++) {
				const auto ledPos = ledPositions->pLedPosition[i];
				auto ledColor = CorsairLedColor();
				ledColor.ledId = ledPos.ledId;
				if (ledPos.left < currWidth) {

					auto posPct = ledPos.left / double(keyboardWidth) * 100;
					
					ledColor.r = 255;
					ledColor.g = 145;
					
					if (posPct >= yellowStop) {
						ledColor.g = 87;
					}

					if (posPct >= lightOrangeStop) {
						ledColor.g = 37;
					}

					if (posPct >= redZone) {
						ledColor.g = 0;
					}

				}
				vec.push_back(ledColor);
			}
			CorsairSetLedsColors(vec.size(), vec.data());

			std::this_thread::sleep_for(std::chrono::milliseconds(75));
			//std::this_thread::sleep_for(std::chrono::seconds(1));
		}// <3
	}
	return 0;
}

