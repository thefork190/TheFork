#include <cstring>
#include <SDL3/SDL_keyboard.h>
#include <ILog.h>
#include "Inputs.h"

namespace Inputs
{
    RawKeboardStates::RawKeboardStates()
    {
        pCur = SDL_GetKeyboardState(&numStates);
        ASSERT(pCur);
        ASSERT(numStates > 0);

        last.reserve(numStates);
        std::memcpy(last.data(), pCur, sizeof(Uint8) * numStates);
    }

    bool RawKeboardStates::WasPressed(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod)
    {
        SDL_Scancode scanCode = SDL_GetScancodeFromKey(keyCode, pKeyMod);
        return WasPressed(scanCode);
    }

    bool RawKeboardStates::WasRelease(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod)
    {
        SDL_Scancode scanCode = SDL_GetScancodeFromKey(keyCode, pKeyMod);
        return WasRelease(scanCode);
    }

    bool RawKeboardStates::WasPressed(SDL_Scancode const scanCode)
    {
        ASSERT(pCur);
        ASSERT(last.size() == numStates);
        return !last[scanCode] && pCur[scanCode];
    }

    bool RawKeboardStates::WasRelease(SDL_Scancode const scanCode)
    {
        ASSERT(pCur);
        ASSERT(last.size() == numStates);
        return last[scanCode] && !pCur[scanCode];
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
                   std::memcpy(pRawKb->last.data(), pRawKb->pCur, sizeof(Uint8) * pRawKb->numStates);

                   if (pRawKb->pCur[SDL_SCANCODE_ESCAPE])
                       LOGF(eDEBUG, "ESC pressed");
                }
            );
    }
}
