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

        last.resize(numStates);
        std::memcpy(last.data(), pCur, sizeof(Uint8) * numStates);
    }

    bool RawKeboardStates::WasPressed(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod) const
    {
        SDL_Scancode scanCode = SDL_GetScancodeFromKey(keyCode, pKeyMod);
        return WasPressed(scanCode);
    }

    bool RawKeboardStates::WasReleased(SDL_Keycode const keyCode, SDL_Keymod* pKeyMod) const
    {
        SDL_Scancode scanCode = SDL_GetScancodeFromKey(keyCode, pKeyMod);
        return WasReleased(scanCode);
    }

    bool RawKeboardStates::WasPressed(SDL_Scancode const scanCode) const
    {
        ASSERT(pCur);
        ASSERT(last.size() == numStates);
        return !last[scanCode] && pCur[scanCode];
    }

    bool RawKeboardStates::WasReleased(SDL_Scancode const scanCode) const
    {
        ASSERT(pCur);
        ASSERT(last.size() == numStates);
        return last[scanCode] && !pCur[scanCode];
    }

    bool RawMouseStates::WasPressed(Uint32 const buttonMask) const
    {
        return !(last.buttons & SDL_BUTTON_LMASK) && (cur.buttons & SDL_BUTTON_LMASK);
    }

    bool RawMouseStates::WasReleased(Uint32 const buttonMask) const
    {
        return (last.buttons & SDL_BUTTON_LMASK) && !(cur.buttons & SDL_BUTTON_LMASK);
    }

    module::module(flecs::world& ecs)
    {
        ecs.module<module>();

        // Create singletons
        ecs.set<RawKeboardStates>(RawKeboardStates());
        ecs.set<RawMouseStates>(RawMouseStates());

        // System to poll states and update singletons
        auto pollStates = ecs.system("Poll Inputs")
            .kind(flecs::OnStore)
            .run([](flecs::iter& it)
                {
                   ASSERTMSG(it.world().has<RawKeboardStates>(), "Raw keyboard states singleton doesn't exist.");
                   auto pRawKb = it.world().get_mut<RawKeboardStates>();
                   std::memcpy(pRawKb->last.data(), pRawKb->pCur, sizeof(Uint8) * pRawKb->numStates);

                   ASSERTMSG(it.world().has<RawMouseStates>(), "Raw mouse states singleton doesn't exist.");
                   auto pRawMouse = it.world().get_mut<RawMouseStates>();
                   pRawMouse->last = pRawMouse->cur;
                   pRawMouse->cur.buttons = SDL_GetMouseState(&pRawMouse->cur.x, &pRawMouse->cur.y);
                }
            );
    }
}
