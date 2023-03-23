#include "stdafx.hpp"

extern "C" __declspec(dllexport) const char *NAME = "ReShade Recorder";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
