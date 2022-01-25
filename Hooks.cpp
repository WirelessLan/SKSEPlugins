#include "Global.h"

std::unordered_map<std::thread::id, std::pair<std::string, std::string>> g_acThreadMap;

RelocAddr <_ActorChangeMeshes> ActorChangeMeshes_HookTarget(0x364FF0);
_ActorChangeMeshes ActorChangeMeshes_Original;

RelocAddr <_SetModelPath> SetModelPath_HookTarget(0xC444C0);
_SetModelPath SetModelPath_Original;

void ActorChangeMeshes_Hook(void* arg1, Actor* arg2) {
	bool isTarget = false;
	std::thread::id threadId = std::this_thread::get_id();

	if (CheckCACSRule(arg2->race->formID, arg2->baseForm->formID)) {
		isTarget = true;
		std::string racePath = GetCACSPath(RuleType::kRuleType_Race, arg2->race->formID);
		std::string actorPath = GetCACSPath(RuleType::kRuleType_Actor, arg2->baseForm->formID);
		g_acThreadMap.insert(std::pair<std::thread::id, std::pair<std::string, std::string>>(threadId, std::pair<std::string, std::string>(racePath, actorPath)));
	}

	ActorChangeMeshes_Original(arg1, arg2);

	if (isTarget)
		g_acThreadMap.erase(threadId);
}

const char* SetModelPath_Hook(void* arg1, UInt64 arg2, const char* subPath, const char* prefixPath) {
	std::thread::id threadId = std::this_thread::get_id();

	auto it = g_acThreadMap.find(threadId);
	if (it != g_acThreadMap.end()) {
		bool checkPath = false;
		size_t subPathLen = _mbstrlen(subPath);
		if (_stricmp(prefixPath, "meshes\\") == 0 && subPathLen >= 4 && _stricmp(&subPath[subPathLen - 4], ".nif") == 0) {
			std::string prefixPathStr = prefixPath;
			std::string subPathStr = subPath;
			std::transform(prefixPathStr.begin(), prefixPathStr.end(), prefixPathStr.begin(), ::tolower);
			std::transform(subPathStr.begin(), subPathStr.end(), subPathStr.begin(), ::tolower);

			size_t prefixPos = subPathStr.find(prefixPathStr);
			if (prefixPos != std::string::npos)
				subPathStr = subPathStr.substr(prefixPos + prefixPathStr.length());

			std::string currentCustomPrefixPath;
			std::string fullPath;

			if (it->second.second != "") {
				fullPath = std::string(prefixPath + it->second.second + subPathStr.c_str());
				if (IsFileExists(fullPath)) {
					currentCustomPrefixPath = prefixPath + it->second.second;
					return SetModelPath_Original(arg1, arg2, subPathStr.c_str(), currentCustomPrefixPath.c_str());
				}
			}

			if (it->second.first != "") {
				fullPath = std::string(prefixPath + it->second.first + subPathStr.c_str());
				if (IsFileExists(fullPath)) {
					currentCustomPrefixPath = prefixPath + it->second.first;
					return SetModelPath_Original(arg1, arg2, subPathStr.c_str(), currentCustomPrefixPath.c_str());
				}
			}
		}
	}

	return SetModelPath_Original(arg1, arg2, subPath, prefixPath);
}

NiExtraData* FindBodyTri(NiAVObject *node, const BSFixedString& name) {
	if (!node)
		return nullptr;

	NiExtraData* triData = node->GetExtraData(name);
	if (triData)
		return triData;

	NiNode* niNode = node->GetAsNiNode();
	if (niNode) {
		for (UInt32 ii = 0; ii < niNode->m_children.m_size; ii++) {
			if (niNode->m_children.m_data[ii]) {
				triData = FindBodyTri(niNode->m_children.m_data[ii], name);
				if (triData)
					return triData;
			}
		}
	}

	return nullptr;
}

class CustomStringExtraData : public NiExtraData {
public:
	CustomStringExtraData();
	~CustomStringExtraData();

	BSFixedString m_pString;	// 18
};

class BodyMorphProcessor : public BSModelDB::BSModelProcessor {
public:
	BodyMorphProcessor(BSModelDB::BSModelProcessor* oldProcessor) : m_oldProcessor(oldProcessor) { }

	virtual void Process(BSModelDB::ModelData* modelData, const char* modelName, NiAVObject** root, UInt32* typeOut) {
		std::thread::id threadId = std::this_thread::get_id();

		auto it = g_acThreadMap.find(threadId);
		if (it != g_acThreadMap.end()) {
			NiAVObject* node = root ? *root : nullptr;
			if (node) {
				node->IncRef();
				NiStringExtraData* stringData = ni_cast(FindBodyTri(node, "BODYTRI"), NiStringExtraData);
				if (stringData) {
					stringData->IncRef();

					CustomStringExtraData* cStringData = (CustomStringExtraData*)stringData;
					bool found = false;
					std::string fullPath, subPath;

					if (it->second.second != "") {
						subPath = it->second.second + std::string(cStringData->m_pString.c_str());
						fullPath = "meshes\\" + subPath;
						if (IsFileExists(fullPath)) {
							found = true;
							cStringData->m_pString = BSFixedString(subPath.c_str());
						}
					}

					if (!found && it->second.first != "") {
						subPath = it->second.first + std::string(cStringData->m_pString.c_str());
						fullPath = "meshes\\" + subPath;
						if (IsFileExists(fullPath))
							cStringData->m_pString = BSFixedString(subPath.c_str());
					}

					stringData->DecRef();
				}
				node->DecRef();
			}
		}

		if (m_oldProcessor)
			m_oldProcessor->Process(modelData, modelName, root, typeOut);
	}

	DEFINE_STATIC_HEAP(Heap_Allocate, Heap_Free)

protected:
	BSModelDB::BSModelProcessor* m_oldProcessor;
};

void SetModelProcessor() {
	(*g_TESProcessor) = new BodyMorphProcessor(*g_TESProcessor);
}

void Hooks_ActorChangeMeshes() {
	struct AiProcess_Code : Xbyak::CodeGenerator {
		AiProcess_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
		{
			Xbyak::Label retnLabel;

			mov(ptr[rsp + 0x08], rcx);
			push(rbp);

			jmp(ptr[rip + retnLabel]);

			L(retnLabel);
			dq(ActorChangeMeshes_HookTarget.GetUIntPtr() + 0x06);
		}
	};
	void* codeBuf = g_localTrampoline.StartAlloc();
	AiProcess_Code code(codeBuf);
	g_localTrampoline.EndAlloc(code.getCurr());

	ActorChangeMeshes_Original = (_ActorChangeMeshes)codeBuf;

	g_branchTrampoline.Write6Branch(ActorChangeMeshes_HookTarget.GetUIntPtr(), (uintptr_t)ActorChangeMeshes_Hook);
}

void Hooks_SetModelPath() {
	struct AiProcess_Code : Xbyak::CodeGenerator {
		AiProcess_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
		{
			Xbyak::Label retnLabel;

			mov(ptr[rsp + 0x08], rbx);
			mov(ptr[rsp + 0x10], rbp);
			mov(ptr[rsp + 0x18], rsi);

			jmp(ptr[rip + retnLabel]);

			L(retnLabel);
			dq(SetModelPath_HookTarget.GetUIntPtr() + 0x0F);
		}
	};
	void* codeBuf = g_localTrampoline.StartAlloc();
	AiProcess_Code code(codeBuf);
	g_localTrampoline.EndAlloc(code.getCurr());

	SetModelPath_Original = (_SetModelPath)codeBuf;

	g_branchTrampoline.Write6Branch(SetModelPath_HookTarget.GetUIntPtr(), (uintptr_t)SetModelPath_Hook);
}