#pragma once

class ExtraLeveledCreature : public BSExtraData {
public:
	ExtraLeveledCreature();
	virtual ~ExtraLeveledCreature();

	UInt64	unk10;		// 10
	TESForm* baseForm;	// 18
};

void trim(std::string& s);
bool IsFileExists(const std::string& path);
std::string GetFileExt(const std::string& fname);
TESForm* GetActorBaseForm(Actor* actor);
TESForm* GetFormFromIdentifier(const std::string& pluginName, const std::string& formIdStr);
TESForm* GetFormFromIdentifier(const std::string& pluginName, const UInt32 formId);