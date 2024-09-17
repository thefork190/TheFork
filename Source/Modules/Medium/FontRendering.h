#pragma once

#include <string>
#include <flecs.h>
#include "LifeCycledModule.h"

namespace FontRendering
{
	// List of available fonts (by name)
	// TODO: make this flexible so we don't have to recompile when available fonts assets change
	enum eAvailableFonts
	{
		// Comic Relief
		COMIC_RELIEF,
		COMIC_RELIEF_BOLD,

		// Crimson
		CRIMSON_BOLD,
		CRIMSON_BOLD_ITALIC,
		CRIMSON_ITALIC,
		CRIMSON_ROMAN,
		CROMSON_SEMI_BOLD,
		CRIMSON_SEMI_BOLD_ITALIC,

		// Hermeneus One
		HERMENEUS_ONE,

		// Inconsolata LGC
		INCONSOLATA_LGC,
		INCONSOLATA_LGC_BOLD,
		INCONSOLATA_LGC_BOLD_ITALIC,
		INCONSOLATA_LGC_ITALIC,

		// Titillium Text
		TITILLIUM_TEXT_BOLD,

		NUM_AVAILABLE_FONTS
	};

	// Component to draw font text
	struct FontText
	{
		std::string text = "";
		eAvailableFonts font = static_cast<eAvailableFonts>(0);
		unsigned int color = 0xFFFFFFFF;
		float fontSize = 16.f;
		float fontSpacing = 0.f;
		float fontBlur = 0.f;
		float posX = 0.f;
		float posY = 0.f;
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
		virtual void OnExit(flecs::world& ecs) override;
	};
}
