#pragma once
#include <skse64/PluginAPI.h>
#include <skse64/GameData.h>
#include <skse64/BSModelDB.h>
#include <skse64/NiNodes.h>
#include <skse64/NiExtraData.h>
#include <skse64/xbyak/xbyak.h>

#include <skse64_common/skse_version.h>
#include <skse64_common/BranchTrampoline.h>

#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <thread>

#include "CACS.h"
#include "Hooks.h"
#include "Utils.h"

#define PLUGIN_NAME	"CustomMeshesPathSSE"
#define PLUGIN_VERSION	MAKE_EXE_VERSION(0, 6, 0)