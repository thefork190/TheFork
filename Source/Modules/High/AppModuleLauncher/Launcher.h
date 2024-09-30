#pragma once

#include <string>
#include <flecs.h>
#include "LifeCycledModule.h"

namespace AppModuleLauncher
{
	class module : public LifeCycledModule
	{
	public:
		// If appModuleName is known, it will be automatically loaded
		module(flecs::world& ecs);
		virtual void OnExit(flecs::world& ecs) override;
		virtual void ProcessEvent(flecs::world& ecs, const SDL_Event* sdlEvent) override;
		virtual void PreProgress(flecs::world& ecs) override;

		static void SetAppModuleToStart(std::string const& name)
		{
			AppModuleToStart() = name;
		}

	private:
		static std::string& AppModuleToStart()
		{
			static std::string appModuleName = "";
			return appModuleName;
		}
	};
}
