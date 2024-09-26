#pragma once

#include <vector>
#include <flecs.h>
#include "LifeCycledModule.h"
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_mouse.h>

// Describes the components that hold the low-level previous and current input states (each device will have their own singleton entities).
// Based on these, higher level modules can handle different specific things (bindings, action types like combos, touch gestures, etc.).

namespace Inputs
{
	struct RawKeboardStates
	{
		std::vector<Uint8> last;  // managed by this struct
		const bool* pCur = nullptr;   // managed by SDL
		int numStates;  // length of arrays above

		RawKeboardStates();

		// Utilities to check if something was just pressed or release
		// pKeyMod will be returned with what the key modifier would be to get the passed in key code
		bool WasPressed(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod = nullptr) const;
		bool WasReleased(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod = nullptr) const;
		bool WasPressed(SDL_Scancode const scanCode) const;
		bool WasReleased(SDL_Scancode const scanCode) const;
	};

	struct RawMouseStates
	{
		struct MouseState
		{
			float x = 0.f;
			float y = 0.f;
			Uint32 buttons = {};
		};
		
		MouseState last = {};
		MouseState cur = {};

		// Utilities to check if something was just pressed or release
		bool WasPressed(Uint32 const buttonMask) const;
		bool WasReleased(Uint32 const buttonMask) const;
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
