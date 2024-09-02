#pragma once

#include <string>
#include <flecs.h>

#ifndef APP_NAME
#define APP_NAME "TheFork"
#endif

namespace HelloTriangle
{
	struct module
	{
		~module();

		module(flecs::world& ecs);
	};
}
