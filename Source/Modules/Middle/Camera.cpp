#include "Camera.h"

namespace Camera
{
    module::module(flecs::world& ecs) 
    {
        ecs.module<module>();

        ecs.component<Camera>();
    }
}