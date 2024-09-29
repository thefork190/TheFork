#pragma once

#include <SDL3/SDL_events.h> // for SDL_Event
#include <flecs.h>

union SDL_Event;

// Base module class with life cycle hooks 
class LifeCycledModule
{
public:
	// Called before exiting (and thus killing the ecs world)
	virtual void OnExit(flecs::world& ecs) {};

    // Called when SDL events get broadcast
    virtual void ProcessEvent(flecs::world& ecs, const SDL_Event* sdlEvent) {};

	// Called prior world progress
	virtual void PreProgress(flecs::world& ecs) {};
};