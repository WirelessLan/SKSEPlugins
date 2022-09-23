#include "Global.h"

ThreadPathMap g_threadPathMap;

typedef void (*_ActorChangeMeshes)(void*, Actor*);
RelocAddr <_ActorChangeMeshes> ActorChangeMeshes_HookTarget(0x364FF0);
_ActorChangeMeshes ActorChangeMeshes_Original;

typedef const char* (*_SetModelPath)(void*, UInt64, const char*, const char*);
RelocAddr <_SetModelPath> SetModelPath_HookTarget(0xC444C0);
_SetModelPath SetModelPath_Original;

void ActorChangeMeshes_Hook(void* arg1, Actor* actor) {
	std::thread::id threadId = std::this_thread::get_id();

	UInt32 raceFormId = actor->race ? actor->race->formID : 0xFFFFFFFF;
	UInt32 baseFormId = actor->baseForm ? actor->baseForm->formID : 0xFFFFFFFF;

	if ((baseFormId >> 24) == 0xFF) {
		TESForm* baseForm = GetActorBaseForm(actor);
		if (baseForm)
			baseFormId = baseForm->formID;
	}

	bool isTarget = false;
	if (CheckCACSRule(RuleType::kRuleType_Race, raceFormId) || CheckCACSRule(RuleType::kRuleType_Actor, baseFormId)) {
		isTarget = true;
		std::string racePath = GetCACSPath(RuleType::kRuleType_Race, raceFormId);
		std::string actorPath = GetCACSPath(RuleType::kRuleType_Actor, baseFormId);
		g_threadPathMap.Add(threadId, { racePath, actorPath });
	}

	ActorChangeMeshes_Original(arg1, actor);

	if (isTarget)
		g_threadPathMap.Delete(threadId);
}

const char* SetModelPath_Hook(void* arg1, UInt64 arg2, const char* subPath, const char* prefixPath) {
	std::thread::id threadId = std::this_thread::get_id();

	const CustomPath* paths = g_threadPathMap.Get(threadId);
	if (paths) {
		if (_stricmp(prefixPath, "meshes\\") == 0 && _stricmp(GetFileExt(subPath).c_str(), "nif") == 0) {
			std::string prefixPathStr = prefixPath;
			std::string subPathStr = subPath;
			std::transform(prefixPathStr.begin(), prefixPathStr.end(), prefixPathStr.begin(), ::tolower);
			std::transform(subPathStr.begin(), subPathStr.end(), subPathStr.begin(), ::tolower);

			size_t prefixPos = subPathStr.find(prefixPathStr);
			if (prefixPos != std::string::npos)
				subPathStr = subPathStr.substr(prefixPos + prefixPathStr.length());

			// Check Actor Path
			if (!paths->actorpath.empty()) {
				std::string currentCustomPrefixPath = prefixPath + paths->actorpath;
				if (IsFileExists("data\\" + currentCustomPrefixPath + subPathStr))
					return SetModelPath_Original(arg1, arg2, subPathStr.c_str(), currentCustomPrefixPath.c_str());
			}

			// Check Race Path
			if (!paths->racePath.empty()) {
				std::string currentCustomPrefixPath = prefixPath + paths->racePath;
				if (IsFileExists("data\\" + currentCustomPrefixPath + subPathStr))
					return SetModelPath_Original(arg1, arg2, subPathStr.c_str(), currentCustomPrefixPath.c_str());
			}
		}
	}

	return SetModelPath_Original(arg1, arg2, subPath, prefixPath);
}

NiExtraData* FindNiExtraDataByName(NiAVObject *node, const BSFixedString& name) {
	if (!node)
		return nullptr;

	NiExtraData* triData = node->GetExtraData(name);
	if (triData)
		return triData;

	NiNode* niNode = node->GetAsNiNode();
	if (niNode) {
		for (UInt32 ii = 0; ii < niNode->m_children.m_size; ii++) {
			if (niNode->m_children.m_data[ii]) {
				triData = FindNiExtraDataByName(niNode->m_children.m_data[ii], name);
				if (triData)
					return triData;
			}
		}
	}

	return nullptr;
}

void CustomModelProcessor::Process(BSModelDB::ModelData* modelData, const char* modelName, NiAVObject** root, UInt32* typeOut) {
	std::thread::id threadId = std::this_thread::get_id();

	const CustomPath* paths = g_threadPathMap.Get(threadId);
	if (paths) {
		NiAVObject* node = root ? *root : nullptr;
		if (node) {
			node->IncRef();

			NiExtraData* bodyTri = FindNiExtraDataByName(node, "BODYTRI");
			if (bodyTri) {
				bodyTri->IncRef();

				NiStringExtraData* stringData = ni_cast(bodyTri, NiStringExtraData);
				if (stringData) {
					CustomStringExtraData* cStringData = reinterpret_cast<CustomStringExtraData*>(stringData);
					bool found = false;
					std::string dataStr = cStringData->m_string.c_str();

					if (!paths->actorpath.empty() && dataStr.find(paths->actorpath) == std::string::npos) {
						std::string subPath = paths->actorpath + dataStr;
						std::string fullPath = "data\\meshes\\" + subPath;
						if (IsFileExists(fullPath)) {
							found = true;
							cStringData->m_string = subPath.c_str();
						}
					}

					if (!found && !paths->racePath.empty() && dataStr.find(paths->racePath) == std::string::npos) {
						std::string subPath = paths->racePath + dataStr;
						std::string fullPath = "data\\meshes\\" + subPath;
						if (IsFileExists(fullPath))
							cStringData->m_string = subPath.c_str();
					}
				}

				bodyTri->DecRef();
			}
			node->DecRef();
		}
	}

	if (m_oldProcessor)
		m_oldProcessor->Process(modelData, modelName, root, typeOut);
}

void ThreadPathMap::Add(std::thread::id key, CustomPath value) {
	std::lock_guard<std::mutex> guard(_mutex);
	_map[key] = value;
}

const CustomPath* ThreadPathMap::Get(std::thread::id key) {
	std::lock_guard<std::mutex> guard(_mutex);
	auto it = _map.find(key);
	if (it == _map.end())
		return nullptr;
	return &it->second;
}

void ThreadPathMap::Delete(std::thread::id key) {
	std::lock_guard<std::mutex> guard(_mutex);
	_map.erase(key);
}

void Hooks_ActorChangeMeshes() {
	struct ActorChangeMeshes_Code : Xbyak::CodeGenerator {
		ActorChangeMeshes_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
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
	ActorChangeMeshes_Code code(codeBuf);
	g_localTrampoline.EndAlloc(code.getCurr());

	ActorChangeMeshes_Original = (_ActorChangeMeshes)codeBuf;

	g_branchTrampoline.Write6Branch(ActorChangeMeshes_HookTarget.GetUIntPtr(), (uintptr_t)ActorChangeMeshes_Hook);
}

void Hooks_SetModelPath() {
	struct SetModelPath_Code : Xbyak::CodeGenerator {
		SetModelPath_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
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
	SetModelPath_Code code(codeBuf);
	g_localTrampoline.EndAlloc(code.getCurr());

	SetModelPath_Original = (_SetModelPath)codeBuf;

	g_branchTrampoline.Write6Branch(SetModelPath_HookTarget.GetUIntPtr(), (uintptr_t)SetModelPath_Hook);
}

void SetModelProcessor() {
	(*g_TESProcessor) = new CustomModelProcessor(*g_TESProcessor);
}