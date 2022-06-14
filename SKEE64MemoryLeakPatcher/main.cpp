// SKSE
#include "common/IDebugLog.h"  // IDebugLog

#include <shlobj.h>	// CSIDL_MYCODUMENTS

#include "Global.h"

PluginHandle			g_pluginHandle = kPluginHandle_Invalid;
SKSEMessagingInterface* g_messaging = nullptr;

void Patch() {
	uintptr_t skee64Ptr = (uintptr_t)GetModuleHandle("skee64.dll");
	if (skee64Ptr == 0) {
		_MESSAGE("No skee64.dll found!");
		return;
	}

	SafeWrite64(skee64Ptr + 0x0017FC10 + 0x8, skee64Ptr + 0x25D49);
}

void OnSKSEMessage(SKSEMessagingInterface::Message* msg) {
	switch (msg->type) {
	case SKSEMessagingInterface::kMessage_DataLoaded:
		Patch();
		break;
	}
}

/* Plugin Query */
extern "C" {
	bool SKSEPlugin_Query(const SKSEInterface* skse, PluginInfo* info) {
		std::string logPath{ "\\My Games\\Skyrim Special Edition\\SKSE\\" PLUGIN_NAME ".log" };
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, logPath.c_str());
		gLog.SetPrintLevel(IDebugLog::kLevel_Error);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = PLUGIN_VERSION;

		if (skse->runtimeVersion != RUNTIME_VERSION_1_5_97) {
			_MESSAGE("unsupported runtime version %d", skse->runtimeVersion);
			return false;
		}

		g_pluginHandle = skse->GetPluginHandle();

		// Get the messaging interface
		g_messaging = (SKSEMessagingInterface*)skse->QueryInterface(kInterface_Messaging);
		if (!g_messaging) {
			_FATALERROR("couldn't get messaging interface");
			return false;
		}

		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface* skse) {
		_MESSAGE("%s Start", PLUGIN_NAME);

		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "SKSE", OnSKSEMessage);

		return true;
	}
};