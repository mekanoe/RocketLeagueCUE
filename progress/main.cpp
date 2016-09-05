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

void* GetProcessBaseAddress() //i can't guarantee that this algorithm is faster butit is shorter :P
{
	HANDLE      processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, getProcessId());
	void*     moduleArray;
	unsigned long* dataSize = new unsigned long;

	if (processHandle)
	{
		if (EnumProcessModules(processHandle, (HMODULE*)&moduleArray, sizeof(void*), dataSize))
		{
			delete dataSize;
			return moduleArray;
		}

		delete dataSize;
		CloseHandle(processHandle);
	}
	return nullptr;
}

int getRocketLeagueTurbo(HANDLE h, void* p) 
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
		// handler isn't initalized
		return 0;
	}
	//+015817E0+120+50+6f4+21c
	std::cout << std::endl << std::hex << GetProcessBaseAddress() << std::endl;
	int base = (int)GetProcessBaseAddress() + 0x0062465C;

	int offsets[6] = { 0x4A0, 0x54, 0x4C, 0x520, 0x54 };

	int addr = base;

	int b;
	ReadProcessMemory(h, (LPVOID)(base), &b, 4, 0);
	addr = b;
	//cout

	int i(0);
	for (int off : offsets) {
		int b;
		ReadProcessMemory(h, (LPVOID)(addr + off), &b, 4, 0);
		addr += b;
		std::cout << i << " = " << std::hex << b << "+" << std::hex << off << " so addr is now " << std::hex << addr << std::endl;
		i++;
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
	
	CorsairPerformProtocolHandshake();
	if (const CorsairError error = CorsairGetLastError()) {
		std::cout << "Handshake failed: " << toString(error) << std::endl;
		getchar();
		return -1;
	}

	const auto ledPositions = CorsairGetLedPositions();
	if (ledPositions && ledPositions->numberOfLed > 0) {
				
		const double keyboardWidth = getKeyboardWidth(ledPositions);
		const int numberOfSteps = 100;
		std::cout << "Working... Press Escape to close program..." << std::endl << std::endl;
		while(!GetAsyncKeyState(VK_ESCAPE)) { //fixed that infinite counter
			
			if (handle == NULL) {
				handle = initProcessHandle();
				pointer = getRocketLeagueTurboPointer(handle);
				if (handle == NULL) {
					std::cout << "Can't find Rocket League. Please start the gaaaaame!" << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(5)); //give the game time to open
					continue;
				}
				else
					std::cout << "Found Rocket League!" << std::endl;
			}

			// XXX: Get this value repeatably. I suck at it.
			// For demos, I used cheat engine to grab the memory locations.
			auto percentage = getRocketLeagueTurbo(handle, (void*)0x32E1FC8C);
			
			std::vector<CorsairLedColor> vec;
			//const auto currWidth = double(keyboardWidth) * (n % (numberOfSteps + 1)) / numberOfSteps;
			const auto currWidth = double(keyboardWidth) * percentage / numberOfSteps;

			for (auto i = 0; i < ledPositions->numberOfLed; i++) {
				const CorsairLedPosition ledPos = ledPositions->pLedPosition[i];
				CorsairLedColor ledColor = CorsairLedColor();
				ledColor.ledId = ledPos.ledId;
				if (ledPos.left < currWidth) {

					double posPct = ledPos.left / keyboardWidth * 100;
					
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
		}// <3
	}
	return 0;
}