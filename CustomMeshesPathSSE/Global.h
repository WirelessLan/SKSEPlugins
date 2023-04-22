#pragma once
#include <skse64/PluginAPI.h>
#include <skse64/GameData.h>
#include <skse64/GameRTTI.h>
#include <skse64/GameExtraData.h>
#include <skse64/NiExtraData.h>
#include <skse64/BSModelDB.h>
#include <skse64/NiNodes.h>
#include <skse64/xbyak/xbyak.h>

#include <skse64_common/skse_version.h>
#include <skse64_common/BranchTrampoline.h>

#include <fstream>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <thread>

#include "CaseInsensitiveMap.h"

#include "Hooks.h"
#include "CACS.h"
#include "Utils.h"

#define PLUGIN_NAME	"CustomMeshesPathSSE"
#define PLUGIN_VERSION	MAKE_EXE_VERSION(0, 7, 0)

extern bool bDebug;