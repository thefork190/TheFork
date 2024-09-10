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

        pLast.reserve(arrLen);
        std::memcpy(pLast.data(), pCur, sizeof(Uint8) * arrLen);
    }

    module::module(flecs::world& ecs)
    {
        ecs.module<module>();

        // Create singletons
        ecs.set<RawKeboardStates>(RawKeboardStates());

        // System to poll states and update singletons
        auto pollStates = ecs.system("Poll Inputs")
            .kind(flecs::OnLoad)
            .run([](flecs::iter& it)
                {
                   ASSERTMSG(it.world().has<RawKeboardStates>(), "Raw keyboard states singleton doesn't exist.");
                   auto pRawKb = it.world().get_mut<RawKeboardStates>();
                   std::memcpy(pRawKb->pLast.data(), pRawKb->pCur, sizeof(Uint8) * pRawKb->arrLen);

                   if (pRawKb->pCur[SDL_SCANCODE_ESCAPE])
                       LOGF(eDEBUG, "ESC pressed");
                }
            );
    }
}
