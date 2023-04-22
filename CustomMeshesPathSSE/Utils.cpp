#include "Global.h"

void Trim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
	}));
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), s.end());
}

std::string ToLower(const std::string& str) {
	std::string retStr = str;
	std::transform(retStr.begin(), retStr.end(), retStr.begin(), [](unsigned char c) {
		return std::tolower(c);
	});
	return retStr;
}

std::string RemovePrefix(const std::string& prefixStr, const std::string& str) {
	size_t prefixPos = str.find(prefixStr);
	if (prefixPos == 0)
		return str.substr(prefixStr.length());
	return str;
}

void Log(const char* fmt, ...) {
	if (bDebug) {
		va_list args;
		va_start(args, fmt);
		gLog.Log(IDebugLog::kLevel_Message, fmt, args);
		va_end(args);
	}
}

namespace BSResource {
	struct ID {
		std::uint32_t file;	// 0
		std::uint32_t ext;	// 4
		std::uint32_t dir;	// 8

		static void GenerateID(ID& id, const char* path) {
			using func_t = void(*)(ID&, const char*);
			func_t func = RelocAddr<func_t>(0xC4A420);
			func(id, path);
		}
	};

	namespace SDirectory2 {
		struct Cursor {
			std::uint64_t	unk00;				// 00
			std::uint64_t	unk08[0xB0 >> 3];	// 08
		};

		void GetReader(SDirectory2::Cursor& cursor) {
			using func_t = void(*)(SDirectory2::Cursor&);
			func_t func = RelocAddr<func_t>(0xC3DEB0);
			func(cursor);
		}

		void ReleaseReader(SDirectory2::Cursor& cursor) {
			using func_t = void(*)(SDirectory2::Cursor&);
			func_t func = RelocAddr<func_t>(0xC3DEE0);
			func(cursor);
		}

		bool Exists(std::uint64_t* cursor, ID& id, std::uint64_t& arg3) {
			using func_t = bool(*)(std::uint64_t*, ID&, std::uint64_t&);
			func_t func = RelocAddr<func_t>(0xC3E560);
			return func(cursor, id, arg3);
		}
	}

	class EntryDB {
	public:
		std::uint64_t	unk00[0x110 >> 3];	// 000
		std::uint64_t	unk110;				// 110
	};

	RelocPtr<EntryDB*> g_entryDB(0x1EBE448);

	ID* AcquireIfExist(ID& id) {
		if (!*g_entryDB)
			return nullptr;

		using func_t = ID*(*)(std::uint64_t&, ID&);
		func_t func = RelocAddr<func_t>(0x1B95A0);
		return func((*g_entryDB)->unk110, id);
	}
}

bool IsFileExists(const std::string& path) {
	std::uint64_t unk00 = 0;
	BSResource::ID file_id;
	BSResource::SDirectory2::Cursor cursor;

	BSResource::ID::GenerateID(file_id, path.c_str());

	BSResource::ID* exist_id = BSResource::AcquireIfExist(file_id);
	if (exist_id)
		return true;

	BSResource::SDirectory2::GetReader(cursor);
	bool retVal = BSResource::SDirectory2::Exists(&cursor.unk08[0], file_id, unk00);
	BSResource::SDirectory2::ReleaseReader(cursor);

	return retVal;
}

std::string GetFileExt(const std::string& fname) {
	size_t idx = fname.rfind('.');
	if (idx == std::string::npos)
		return std::string();
	return fname.substr(idx + 1);
}

class ExtraLeveledCreature : public BSExtraData {
public:
	std::uint64_t	unk10;		// 10
	TESForm*		baseForm;	// 18
};

TESForm* GetActorBaseForm(Actor* actor) {
	if (!actor)
		return nullptr;

	BSExtraData* extraData = actor->extraData.GetByType(ExtraDataType::kExtraData_LeveledCreature);
	if (!extraData)
		return nullptr;

	ExtraLeveledCreature* extraLeveldCreature = DYNAMIC_CAST(extraData, BSExtraData, ExtraLeveledCreature);
	if (!extraLeveldCreature)
		return nullptr;

	return extraLeveldCreature->baseForm;
}

TESForm* GetFormFromIdentifier(const std::string& pluginName, const std::string& formIdStr) {
	std::uint32_t formID = std::stoul(formIdStr, nullptr, 16) & 0xFFFFFF;
	return GetFormFromIdentifier(pluginName, formID);
}

TESForm* GetFormFromIdentifier(const std::string& pluginName, const std::uint32_t formId) {
	if (!*g_dataHandler)
		return nullptr;

	const ModInfo* mod = (*g_dataHandler)->LookupModByName(pluginName.c_str());
	if (!mod || mod->modIndex == -1)
		return nullptr;

	std::uint32_t actualFormId = formId;
	std::uint32_t pluginIndex = mod->GetPartialIndex();
	if (!mod->IsLight())
		actualFormId |= pluginIndex << 24;
	else
		actualFormId |= pluginIndex << 12;

	return LookupFormByID(actualFormId);
}