#pragma once

#include <flecs.h>

// Base module class with life cycle hooks 
class LifeCycledModule
{
public:
	// Called before exiting (and thus killing the ecs world)
	virtual void OnExit(flecs::world& ecs) {};
};