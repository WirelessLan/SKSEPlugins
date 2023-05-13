#include "Hooks.h"

#include <thread>
#include <mutex>
#include <unordered_map>

#include <skse64/GameRTTI.h>
#include <skse64/GameReferences.h>
#include <skse64/NiNodes.h>
#include <skse64/NiExtraData.h>
#include <skse64/BSModelDB.h>
#include <skse64/xbyak/xbyak.h>
#include <skse64_common/BranchTrampoline.h>

#include "CACS.h"
#include "Utils.h"

namespace Hooks {
	template <class _key, class _value>
	class ThreadSafeMap {
	public:
		ThreadSafeMap() {}
		void Add(_key key, _value value) {
			std::lock_guard<std::mutex> guard(_mutex);
			_map[key] = value;
		}
		_value* Get(const _key& key) {
			std::lock_guard<std::mutex> guard(_mutex);
			auto it = _map.find(key);
			if (it == _map.end())
				return nullptr;
			return &it->second;
		}
		void Delete(_key key) {
			std::lock_guard<std::mutex> guard(_mutex);
			_map.erase(key);
		}
		void Clear() {
			std::lock_guard<std::mutex> guard(_mutex);
			_map.clear();
		}
	private:
		std::unordered_map<_key, _value> _map;
		std::mutex _mutex;
	};

	struct FormPath {
		CACS::RuleType RuleType;
		std::uint32_t FormID;
		std::string Path;
	};

	ThreadSafeMap<std::thread::id, std::vector<FormPath>> g_threadPathMap;
	ThreadSafeMap<std::string, std::string> g_fullPathMap;

	using ReplaceRefModel_t = void(*)(TESForm&, TESObjectREFR*);
	RelocAddr<ReplaceRefModel_t> ReplaceRefModel_Target(0x364FF0);
	ReplaceRefModel_t ReplaceRefModel;

	using PrepareName_t = const char* (*)(char*, std::uint32_t, const char*, const char*);
	RelocAddr<PrepareName_t> PrepareName_Target(0xC444C0);
	PrepareName_t PrepareName;

	void ReplaceRefModel_Hook(TESForm& a_npc, TESObjectREFR* a_ref) {
		Actor* actor = DYNAMIC_CAST(a_ref, TESObjectREFR, Actor);
		if (!actor) {
			ReplaceRefModel(a_npc, a_ref);
			return;
		}

		std::thread::id threadId = std::this_thread::get_id();
		std::vector<FormPath> pathVec;

		std::uint32_t baseFormId = Utils::GetBaseFormID(actor);
		if (CACS::CheckCACSRule(CACS::RuleType::kRuleType_Actor, baseFormId)) {
			std::string actorPath = CACS::GetCACSPath(CACS::RuleType::kRuleType_Actor, baseFormId);
			pathVec.push_back({ CACS::RuleType::kRuleType_Actor, baseFormId, actorPath });
		}

		std::uint32_t raceFormId = actor->race ? actor->race->formID : 0;
		if (CACS::CheckCACSRule(CACS::RuleType::kRuleType_Race, raceFormId)) {
			std::string racePath = CACS::GetCACSPath(CACS::RuleType::kRuleType_Race, raceFormId);
			pathVec.push_back({ CACS::RuleType::kRuleType_Race, raceFormId, racePath });
		}

		if (!pathVec.empty())
			g_threadPathMap.Add(threadId, pathVec);

		ReplaceRefModel(a_npc, a_ref);

		if (!pathVec.empty())
			g_threadPathMap.Delete(threadId);
	}

	const char* PrepareName_Hook(char* arg1, std::uint32_t arg2, const char* a_subPath, const char* a_prefixPath) {
		if (_stricmp(a_prefixPath, "meshes\\") != 0 || _stricmp(Utils::GetFileExt(a_subPath).c_str(), "nif") != 0)
			return PrepareName(arg1, arg2, a_subPath, a_prefixPath);

		std::thread::id threadId = std::this_thread::get_id();

		const std::vector<FormPath>* cPath = g_threadPathMap.Get(threadId);
		if (!cPath)
			return PrepareName(arg1, arg2, a_subPath, a_prefixPath);

		for (const FormPath& path : *cPath) {
			std::string customPrefixPath, customSubPath, customPath;

			if (CACS::SetCustomPaths(path.RuleType, path.FormID, path.Path, a_prefixPath, a_subPath, customPrefixPath, customSubPath, customPath)) {
				g_fullPathMap.Add(customPath, path.Path);
				return PrepareName(arg1, arg2, customSubPath.c_str(), customPrefixPath.c_str());
			}
		}

		return PrepareName(arg1, arg2, a_subPath, a_prefixPath);
	}

	NiExtraData* FindNiExtraDataByName(NiAVObject* a_node, const BSFixedString& a_name) {
		if (!a_node)
			return nullptr;

		NiExtraData* triData = a_node->GetExtraData(a_name);
		if (triData)
			return triData;

		NiNode* niNode = a_node->GetAsNiNode();
		if (niNode) {
			for (std::uint32_t ii = 0; ii < niNode->m_children.m_size; ii++) {
				if (niNode->m_children.m_data[ii]) {
					triData = FindNiExtraDataByName(niNode->m_children.m_data[ii], a_name);
					if (triData)
						return triData;
				}
			}
		}

		return nullptr;
	}

	class CustomStringExtraData : public NiExtraData {
	public:
		BSFixedString m_string;		// 18
	};

	class CustomModelProcessor : public BSModelDB::BSModelProcessor {
	public:
		CustomModelProcessor(BSModelDB::BSModelProcessor* oldProcessor) : m_oldProcessor(oldProcessor) { }

		virtual void Process(BSModelDB::ModelData* a_modelData, const char* a_modelName, NiAVObject** a_root, UInt32* a_typeOut) {
			if (a_modelName) {
				std::string modelPath = Utils::RemovePrefix(Utils::ReplaceSlash(Utils::ToLower(a_modelName)), "data\\");
				std::string* cPath = g_fullPathMap.Get(modelPath);
				if (cPath && a_root && *a_root) {
					NiAVObject* node = *a_root;
					node->IncRef();

					NiExtraData* bodyTri = FindNiExtraDataByName(node, "BODYTRI");
					if (bodyTri) {
						bodyTri->IncRef();

						NiStringExtraData* stringData = ni_cast(bodyTri, NiStringExtraData);
						if (stringData) {
							CustomStringExtraData* cStringData = reinterpret_cast<CustomStringExtraData*>(stringData);
							std::string dataStr = Utils::ReplaceSlash(Utils::ToLower(cStringData->m_string.c_str()));
							if (Utils::StartsWith(dataStr, "meshes\\"))
								dataStr = "meshes\\" + *cPath + Utils::RemovePrefix(dataStr, "meshes\\");
							else
								dataStr = *cPath + dataStr;

							cStringData->m_string = dataStr.c_str();
						}

						bodyTri->DecRef();
					}

					node->DecRef();
				}
			}

			if (m_oldProcessor)
				m_oldProcessor->Process(a_modelData, a_modelName, a_root, a_typeOut);
		}

		DEFINE_STATIC_HEAP(Heap_Allocate, Heap_Free)

	protected:
		BSModelDB::BSModelProcessor* m_oldProcessor;
	};

	void SetModelProcessor() {
		static bool bSet = false;
		if (!bSet) {
			(*g_TESProcessor) = new CustomModelProcessor(*g_TESProcessor);
			bSet = true;
		}
	}

	void Hooks_ReplaceRefModel() {
		struct asm_code : Xbyak::CodeGenerator {
			asm_code(void* buf) : Xbyak::CodeGenerator(4096, buf) {
				Xbyak::Label retnLabel;

				mov(ptr[rsp + 0x08], rcx);
				push(rbp);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(ReplaceRefModel_Target.GetUIntPtr() + 0x06);
			}
		};

		void* codeBuf = g_localTrampoline.StartAlloc();
		asm_code code(codeBuf);
		g_localTrampoline.EndAlloc(code.getCurr());

		ReplaceRefModel = (ReplaceRefModel_t)codeBuf;

		g_branchTrampoline.Write6Branch(ReplaceRefModel_Target.GetUIntPtr(), (std::uintptr_t)ReplaceRefModel_Hook);
	}

	void Hooks_PrepareName() {
		struct asm_code : Xbyak::CodeGenerator {
			asm_code(void* buf) : Xbyak::CodeGenerator(4096, buf) {
				Xbyak::Label retnLabel;

				mov(ptr[rsp + 0x08], rbx);
				mov(ptr[rsp + 0x10], rbp);
				mov(ptr[rsp + 0x18], rsi);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(PrepareName_Target.GetUIntPtr() + 0x0F);
			}
		};

		void* codeBuf = g_localTrampoline.StartAlloc();
		asm_code code(codeBuf);
		g_localTrampoline.EndAlloc(code.getCurr());

		PrepareName = (PrepareName_t)codeBuf;

		g_branchTrampoline.Write6Branch(PrepareName_Target.GetUIntPtr(), (std::uintptr_t)PrepareName_Hook);
	}

	void ClearPathMap() {
		g_threadPathMap.Clear();
		g_fullPathMap.Clear();
	}
}