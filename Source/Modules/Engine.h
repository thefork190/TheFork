#pragma once

#include <string>
#include <flecs.h>

#ifndef APP_NAME
#define APP_NAME "TheFork"
#endif

namespace Engine
{
	// Describes an area the entity wants to display content on (could be something like an app window)
	struct Canvas
	{
		unsigned int creationWidth = 256;
		unsigned int creationHeight = 256;
	};

	// Contains general and commonly used data related to the current state(s) of the engine
	// The creation of the context singleton kickstarts the whole engine
	struct Context
	{
		std::string appName = APP_NAME;
	};
	
	struct module
	{
		module(flecs::world& ecs); // Ctor that loads the module
	};


	// Creates the required components to start getting systems to run
	void KickstartEngine(flecs::world& ecs);
}
