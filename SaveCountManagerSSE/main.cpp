#include <common/IDebugLog.h>

#include <skse64/PluginAPI.h>
#include <skse64/GameSettings.h>

#include <skse64_common/skse_version.h>

#include <shlobj.h>	// CSIDL_MYCODUMENTS
#include <vector>
#include <algorithm>

#define PLUGIN_NAME	"SaveCountManagerSSE"

PluginHandle			g_pluginHandle = kPluginHandle_Invalid;
SKSEMessagingInterface* g_messaging = NULL;

uint32_t uMaxSaveCnt = 20;
bool bPreserveFirstSave = false;

std::string GetINIOption(const char* section, const char* key) {
	std::string	result;
	char resultBuf[256] = { 0 };

	static const std::string configPath{ "Data\\SKSE\\Plugins\\" PLUGIN_NAME ".ini" };
	GetPrivateProfileStringA(section, key, NULL, resultBuf, sizeof(resultBuf), configPath.c_str());
	return resultBuf;
}

void ReadINI() {
	std::string uMaxSaveCnt_value = GetINIOption("Settings", "uMaxSaveCnt");
	if (!uMaxSaveCnt_value.empty() && std::stoul(uMaxSaveCnt_value)) {
		uMaxSaveCnt = std::stoul(uMaxSaveCnt_value);
		_MESSAGE("uMaxSaveCnt: %u", uMaxSaveCnt);
	}

	std::string bPreserveFirstSave_value = GetINIOption("Settings", "bPreserveFirstSave");
	if (!bPreserveFirstSave_value.empty()) {
		bPreserveFirstSave = std::stoul(bPreserveFirstSave_value);
		_MESSAGE("bPreserveFirstSave: %u", bPreserveFirstSave);
	}
}

class SaveFile {
public:
	enum SAVE_TYPE : uint16_t {
		kRegular,
		kAuto,
		kQuick,
		kExit
	};

	SaveFile() {}

	SaveFile(std::string saveName) {
		this->saveName = saveName;
		separateSaveName();
	}

	bool operator < (const SaveFile& sfi) const	{
		return (number < sfi.number);
	}

	std::string saveName;

	SAVE_TYPE type;
	uint32_t number;
	std::string	id;
	uint16_t modded;
	std::string playerName;
	std::string location;
	std::string unk2;
	std::string time;
	uint32_t unk3;
	uint32_t unk4;

private:

	void separateSaveName() {
		this->cIdx = 0;
		getSaveType();
		getSaveNumber();
		getSaveId();
		getIsModded();
		getPlayerName();
		getLocation();
		getUnk2();
		getTime();
		getUnk3();
		getUnk4();
	}

	void getNext() {
		UInt32 ii;
		for (ii = 0; this->cIdx < this->saveName.length(); ii++) {
			if (this->saveName[this->cIdx] == '_') {
				this->cIdx++;
				break;
			}
			buf[ii] = getNextChar();
		}
		buf[ii] = 0;
	}

	std::string getNextString() {
		getNext();
		return std::string(buf);
	}

	UInt32 getNextInt() {
		getNext();
		return atoi(buf);
	}

	char getNextChar() {
		return this->saveName[this->cIdx++];
	}

	void getSaveType() {
		char saveType[30] = { 0 };
		for (UInt32 ii = 0; this->cIdx < this->saveName.length(); ii++) {
			if (this->saveName[this->cIdx] >= '0' && this->saveName[this->cIdx] <= '9')
				break;
			saveType[ii] = getNextChar();
		}

		if (strcmp(saveType, "Save") == 0) this->type = SAVE_TYPE::kRegular;
		else if (strcmp(saveType, "Autosave") == 0) this->type = SAVE_TYPE::kAuto;
		else if (strcmp(saveType, "Quicksave") == 0) this->type = SAVE_TYPE::kQuick;
		else if (strcmp(saveType, "Exitsave") == 0)	this->type = SAVE_TYPE::kExit;
		else
			_WARNING("Unknown Save Type[%s]", saveType);
	}

	void getSaveNumber() {
		this->number = getNextInt();
	}

	void getSaveId() {
		this->id = getNextString();
	}

	void getIsModded() {
		this->modded = getNextInt();
	}

	void getPlayerName() {
		this->playerName = getNextString();
	}

	void getLocation() {
		this->location = getNextString();
	}

	void getUnk2() {
		this->unk2 = getNextString();
	}

	void getTime() {
		this->time = getNextString();
	}

	void getUnk3() {
		this->unk3 = getNextInt();
	}
	
	void getUnk4() {
		this->unk4 = getNextInt();
	}

	uint32_t cIdx;
	char buf[300];
};

class SaveCountManager {
public:
	SaveCountManager(const std::string &saveName) {
		_MESSAGE("New save file name: %s", saveName.c_str());

		this->sf = SaveFile(saveName);
		_MESSAGE("type[%d] number[%d] id[%s] modded[%d] playerName[%s] location[%s] unk2[%s] time[%s] unk3[%d] unk4[%d]", 
				this->sf.type, this->sf.number, this->sf.id.c_str(), this->sf.modded, this->sf.playerName.c_str(), 
				this->sf.location.c_str(), this->sf.unk2.c_str(), this->sf.time.c_str(), this->sf.unk3, this->sf.unk4);
	}

	std::map<uint32_t, std::vector<std::string>> getSaveMap() {
		std::map<uint32_t, std::vector<std::string>> retMap;

		std::string saveDir = getSavePath();
		if (saveDir.empty())
			return retMap;

		WIN32_FIND_DATAA findData;
		HANDLE hFind = INVALID_HANDLE_VALUE;

		std::string targets = saveDir + "\\Save*_" + this->sf.id + "*";

		hFind = FindFirstFileA(targets.c_str(), &findData);
		if (hFind == INVALID_HANDLE_VALUE)
			return retMap;

		do {
			std::string fileName = findData.cFileName;
			SaveFile save = SaveFile(fileName);
			auto num_iter = retMap.find(save.number);
			if (num_iter == retMap.end())
				retMap.insert(std::make_pair(save.number, std::vector<std::string>{ fileName }));
			else
				num_iter->second.push_back(fileName);
		} 
		while (FindNextFileA(hFind, &findData) != 0);
		FindClose(hFind);

		return retMap;
	}

	void DeleteOldSaveFiles() {
		if (this->sf.type != SaveFile::SAVE_TYPE::kRegular)
			return;

		std::map<uint32_t, std::vector<std::string>> sv_map = getSaveMap();
		if ((uint32_t)sv_map.size() < uMaxSaveCnt)
			return;

		uint32_t map_idx = 0;
		for (auto iter : sv_map) {
			if (map_idx >= sv_map.size() - uMaxSaveCnt)
				break;

			if (bPreserveFirstSave && map_idx == 0) {
				map_idx++;
				continue;
			}

			for (auto p_iter : iter.second) {
				if (!deleteSaveFile(p_iter)) {
					_MESSAGE("Failed to remove file: %s", p_iter.c_str());
					continue;
				}
			}

			map_idx++;
		}
	}

private:
	std::string getSavePath() {
		char path[MAX_PATH];
		ASSERT(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path)));

		static std::string saveDir;
		if (!saveDir.empty())
			return saveDir;

		saveDir = std::string(path) + "\\My Games\\Skyrim Special Edition\\";

		Setting* localSavePath = GetINISetting("sLocalSavePath:General");
		if (localSavePath && (localSavePath->GetType() == Setting::kType_String))
			saveDir += localSavePath->data.s;
		else
			saveDir += "Saves\\";

		return saveDir;
	}

	std::string getRawName(const std::string& filePath) {
		size_t lastindex = filePath.find_last_of(".");
		if (lastindex == std::string::npos)
			return filePath;
		return getRawName(filePath.substr(0, lastindex));
	}

	bool deleteSaveFile(std::string& saveName) {
		std::string saveDir = getSavePath();
		std::string savePath = saveDir + saveName;
		_MESSAGE("delete file: %s", saveName.c_str());

		if (!DeleteFileA(savePath.c_str()))
			return false;
		
		_MESSAGE("%s deleted!", saveName.c_str());
		return true;
	}

	SaveFile sf;
};

void OnSKSEMessage(SKSEMessagingInterface::Message* msg) {
	switch (msg->type) {
	case SKSEMessagingInterface::kMessage_SaveGame:
		SaveCountManager scm((const char *)msg->data);
		scm.DeleteOldSaveFiles();
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
		info->version = MAKE_EXE_VERSION(1, 0, 0);

		g_pluginHandle = skse->GetPluginHandle();

		// Get the messaging interface
		g_messaging = (SKSEMessagingInterface*)skse->QueryInterface(kInterface_Messaging);
		if (!g_messaging) {
			_MESSAGE("couldn't get messaging interface");
			return false;
		}

		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface* skse) {
		_MESSAGE("%s Loaded", PLUGIN_NAME);

		ReadINI();

		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "SKSE", OnSKSEMessage);

		return true;
	}
};