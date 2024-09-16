#pragma once

#include <flecs.h>
#include "LifeCycledModule.h"

namespace FlappyClone
{
	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs);
		virtual void OnExit(flecs::world& ecs) override;
	};
}
