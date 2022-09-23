#pragma once

struct CustomPath {
	std::string racePath;
	std::string actorpath;
};

class ThreadPathMap {
public:
	ThreadPathMap() {}
	void Add(std::thread::id key, CustomPath value);
	const CustomPath* Get(std::thread::id key);
	void Delete(std::thread::id key);

private:
	std::unordered_map<std::thread::id, CustomPath> _map;
	std::mutex _mutex;
};

class CustomStringExtraData : public NiExtraData {
public:
	CustomStringExtraData();
	~CustomStringExtraData();

	BSFixedString m_string;	// 18
};

class CustomModelProcessor : public BSModelDB::BSModelProcessor {
public:
	CustomModelProcessor(BSModelDB::BSModelProcessor* oldProcessor) : m_oldProcessor(oldProcessor) { }
	virtual void Process(BSModelDB::ModelData* modelData, const char* modelName, NiAVObject** root, UInt32* typeOut);
	DEFINE_STATIC_HEAP(Heap_Allocate, Heap_Free)

protected:
	BSModelDB::BSModelProcessor* m_oldProcessor;
};

void Hooks_ActorChangeMeshes();
void Hooks_SetModelPath();
void SetModelProcessor();