#include <cstring>
#include <SDL3/SDL_keyboard.h>
#include <ILog.h>
#include "Inputs.h"

namespace Inputs
{
    RawKeboardStates::RawKeboardStates()
    {
        pCur = SDL_GetKeyboardState(&arrLen);
        ASSERT(pCur);
        ASSERT(arrLen > 0);

        pLast = new Uint8[arrLen];
        std::memcpy(pLast, pCur, sizeof(Uint8) * arrLen);
    }

    RawKeboardStates::~RawKeboardStates()
    {
        if (pLast)
            delete[] pLast;
    }

    module::module(flecs::world& ecs)
    {
        ecs.module<module>();

        // Create singletons
        ecs.set<RawKeboardStates>({});

        // System to poll states and update singletons
        auto pollStates = ecs.system("Poll Inputs")
            .kind(flecs::OnLoad)
            .run([](flecs::iter& it)
                {
                   
                }
            );
    }
}
