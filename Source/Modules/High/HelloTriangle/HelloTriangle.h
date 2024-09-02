#pragma once

#include <string>
#include <flecs.h>
#include "LifeCycledModule.h"

namespace HelloTriangle
{
	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs);
		virtual void OnExit(flecs::world& ecs) override;
	};
}
