#pragma once

#include <functional>
#include <flecs.h>
#include "LifeCycledModule.h"
#include "Medium/FontRendering.h"

// Remaining Todos:
// - [ ] Using different fonts (and sizes)
// - [ ] Being able to use external textures
// - [ ] Address non 1x DPI scale (including fonts)
// - [ ] Address DPI changes (per monitor)
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
	};

	// Forwards SDL events
	void ForwardEvent(flecs::world& ecs, const SDL_Event* sdlEvent);
	
	// Checks if UI is currently capturing inputs (in which case the app probably shouldn't handle inputs)
	bool WantsCaptureInputs(flecs::world& ecs);

	// This function will always return a valid ImFont* if the UI is currently initialized.
	// A nullptr represents the fallback font (which should always be valid for usage).
	// If the specified font was not yet loaded, it will be loaded at a deferred time and the default fallback font will be returned.
	ImFont* GetOrAddFont(flecs::world& ecs, FontRendering::eAvailableFonts const font, float const size);
}
