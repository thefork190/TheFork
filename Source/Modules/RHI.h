#pragma once

#include <IGraphics.h>
#include <RingBuffer.h>
#include <flecs.h>

// RHI module provides the lowest level components required to drive rendering.
// It uses The Forge as to support cross-platform backends.

namespace RHI
{
	// RHI component is a singleton and holds global data used for rendering
	struct RHI
	{
		// Constructor to init the renderer
		RHI();

		// Destructor to kill it
		~RHI();

		Renderer* pRenderer = nullptr;
		unsigned int dataBufferCount = 2; // 1 frame in flight and one being updated on CPU
		unsigned int frameIndex = 0;
		Queue* pGfxQueue = nullptr;
		GpuCmdRing gfxCmdRing = {};
	};

	// Swapchains will usually relate to a window entity
	struct Swapchain
	{
		SwapChain* pSwapChain = nullptr;
	};
	
	struct module
	{
		module(flecs::world& ecs); // Ctor that loads the module
	};

	// Creates the RHI singleton
	bool CreateRHI(flecs::world& ecs);
}
