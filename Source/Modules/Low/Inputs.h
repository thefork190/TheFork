#pragma once

#include <flecs.h>
#include "LifeCycledModule.h"

#include <SDL3/SDL_stdinc.h>

// Describes the components that hold the low-level previous and current input states (each device will have their own singleton entities).
// Based on these, higher level modules can handle different specific things (bindings, action types like combos, touch gestures, etc.).

namespace Inputs
{
	struct RawKeboardStates
	{
		Uint8* pLast = nullptr;  // managed by this struct
		const Uint8* pCur = nullptr;   // managed by SDL
		int arrLen;  // length of arrays above

		RawKeboardStates();
		~RawKeboardStates();
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
