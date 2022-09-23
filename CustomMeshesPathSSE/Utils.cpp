#include "Global.h"

void trim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
		}));
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

bool IsFileExists(const std::string& path) {
	if (path.empty())
		return false;
	DWORD dwAttrib = GetFileAttributes(path.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string GetFileExt(const std::string& fname) {
	size_t idx = fname.rfind('.');
	if (idx == std::string::npos)
		return std::string();
	return fname.substr(idx + 1);
}

TESForm* GetActorBaseForm(Actor* actor) {
	if (!actor)
		return nullptr;

	BSExtraData* extraData = actor->extraData.m_data;
	while (extraData) {
		ExtraLeveledCreature* extraLeveledCreature = DYNAMIC_CAST(extraData, BSExtraData, ExtraLeveledCreature);
		if (extraLeveledCreature)
			return extraLeveledCreature->baseForm;

		extraData = extraData->next;
	}

	return nullptr;
}

TESForm* GetFormFromIdentifier(const std::string& pluginName, const std::string& formIdStr) {
	UInt32 formID = std::stoul(formIdStr, nullptr, 16) & 0xFFFFFF;
	return GetFormFromIdentifier(pluginName, formID);
}

TESForm* GetFormFromIdentifier(const std::string& pluginName, const UInt32 formId) {
	if (!*g_dataHandler)
		return nullptr;

	const ModInfo* mod = (*g_dataHandler)->LookupModByName(pluginName.c_str());
	if (!mod || mod->modIndex == -1)
		return nullptr;

	UInt32 actualFormId = formId;
	UInt32 pluginIndex = mod->GetPartialIndex();
	if (!mod->IsLight())
		actualFormId |= pluginIndex << 24;
	else
		actualFormId |= pluginIndex << 12;

	return LookupFormByID(actualFormId);
}