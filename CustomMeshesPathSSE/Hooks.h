#pragma once
#include "Global.h"

typedef void (*_ActorChangeMeshes)(void*, Actor*);
typedef const char* (*_SetModelPath)(void*, UInt64, const char*, const char*);

void Hooks_ActorChangeMeshes();
void Hooks_SetModelPath();
void SetModelProcessor();