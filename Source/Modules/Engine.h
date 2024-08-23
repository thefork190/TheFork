#pragma once

#include <string>
#include <flecs.h>

#ifndef APP_NAME
#define APP_NAME "The Fork"
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
	struct Context
	{
		std::string appName = APP_NAME;
		bool isRunning = true;
	};
	
	struct module
	{
		module(flecs::world& ecs); // Ctor that loads the module
	};


	// Creates the required components to start getting systems to run
	void KickstartEngine(flecs::world& ecs);
}
