#pragma once

#include <string>
#include <flecs.h>
#include "LifeCycledModule.h"

#ifndef APP_NAME
#define APP_NAME "The Fork"
#endif

namespace Engine
{
	// Describes an area the entity wants to display content on (could be something like an app window)
	struct Canvas
	{
		unsigned int width = 256;
		unsigned int height = 256;
	};

	// Contains general and commonly used data related to the current state(s) of the engine
	// The creation of the context singleton kickstarts the whole engine
	class Context
	{
	private:
		std::string mAppName = APP_NAME;
		bool mRequestedExit = false;

	public:
		std::string const AppName() const { return mAppName; }
		void SetAppName(std::string const& appName) { mAppName = appName; }
		void RequestExit() { mRequestedExit = true; }
		bool const HasRequestedExit() const { return mRequestedExit; }
	};
	
	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
	};


	// Creates the required components to start getting systems to run
	void KickstartEngine(flecs::world& ecs, std::string const* pAppName = nullptr);
}
