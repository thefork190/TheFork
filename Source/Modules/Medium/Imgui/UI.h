#pragma once

#include <functional>
#include <flecs.h>
#include "LifeCycledModule.h"
#include "Medium/FontRendering.h"

// Implemented features:
// - [X] Using different fonts (and sizes)
// - [X] Being able to use external textures
// - [X] Address non 1x DPI scale at init time (including fonts)
// - [X] Address DPI changes at runtime (OS settings change and per monitor)
// - [ ] Multi-viewport

struct ImFont;

namespace UI
{
	struct UI
	{
		std::function<void(flecs::world&)> Update;
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
		virtual void OnExit(flecs::world& ecs) override;
        virtual void ProcessEvent(flecs::world& ecs, const SDL_Event* sdlEvent) override;
	};

	// Checks if UI is currently capturing inputs (in which case the app probably shouldn't handle inputs)
	bool WantsCaptureInputs(flecs::world& ecs);

	// This function will always return a valid ImFont* if the UI is currently initialized.
	// A nullptr can be returned if UI is not initialized or if the default font failed to load for some reason.  In that case, don't call on ImGui::Push/PopFont()
	// If the specified font was not yet loaded, it will be loaded at a deferred time and the default fallback font will be returned.
	ImFont* GetOrAddFont(flecs::world& ecs, FontRendering::eAvailableFonts const font, float const size);
}
