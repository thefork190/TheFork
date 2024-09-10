#pragma once

#include <vector>
#include <flecs.h>
#include "LifeCycledModule.h"
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>

// Describes the components that hold the low-level previous and current input states (each device will have their own singleton entities).
// Based on these, higher level modules can handle different specific things (bindings, action types like combos, touch gestures, etc.).

namespace Inputs
{
	struct RawKeboardStates
	{
		std::vector<Uint8> last;  // managed by this struct
		const Uint8* pCur = nullptr;   // managed by SDL
		int arrLen;  // length of arrays above

		RawKeboardStates();

		// Utilities to check if something was just pressed or release
		bool WasPressed(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod = nullptr);
		bool WasRelease(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod = nullptr);
		bool WasPressed(SDL_Scancode const scanCode);
		bool WasRelease(SDL_Scancode const scanCode);
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
