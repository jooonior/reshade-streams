#include "stdafx.hpp"
#include "addon.hpp"

static void on_init_effect_runtime(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->create_private_data<runtime_data>();
}

static void on_destroy_effect_runtime(reshade::api::effect_runtime *runtime)
{
	runtime->destroy_private_data<runtime_data>();
}

extern "C" __declspec(dllexport) const char *NAME = "ReShade Recorder";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
