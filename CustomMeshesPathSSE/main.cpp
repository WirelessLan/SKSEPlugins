// F4SE
#include "common/IDebugLog.h"  // IDebugLog

#include <shlobj.h>	// CSIDL_MYCODUMENTS

#include "Global.h"

PluginHandle			g_pluginHandle = kPluginHandle_Invalid;
SKSEMessagingInterface* g_messaging = NULL;

void OnSKSEMessage(SKSEMessagingInterface::Message* msg) {
	switch (msg->type) {
	case SKSEMessagingInterface::kMessage_DataLoaded:
		SetModelProcessor();
		break;
	case SKSEMessagingInterface::kMessage_NewGame:
	case SKSEMessagingInterface::kMessage_PreLoadGame:
		_MESSAGE("Load Rules...");
		LoadRules();
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
			_MESSAGE("couldn't get messaging interface");
			return false;
		}

		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface* f4se) {
		_MESSAGE("%s Loaded", PLUGIN_NAME);

		if (!g_branchTrampoline.Create(1024 * 64)) {
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}
		if (!g_localTrampoline.Create(1024 * 64, nullptr))
		{
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "SKSE", OnSKSEMessage);

		Hooks_ActorChangeMeshes();
		Hooks_SetModelPath();

		return true;
	}
};