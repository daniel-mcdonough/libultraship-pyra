#include "ship/controller/controldevice/controller/mapping/sdl/SDLTouchpadWedgeToButtonMapping.h"

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>
#include <cmath>

#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/utils/StringHelper.h"

namespace Ship {

SDLTouchpadWedgeToButtonMapping::SDLTouchpadWedgeToButtonMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask,
                                                                 int32_t touchpadIndex, float startAngleDeg,
                                                                 float endAngleDeg, float innerRadius)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerButtonMapping(PhysicalDeviceType::SDLGamepad, portIndex, bitmask), mTouchpadIndex(touchpadIndex),
      mStartAngleDeg(startAngleDeg), mEndAngleDeg(endAngleDeg), mInnerRadius(innerRadius) {
}

void SDLTouchpadWedgeToButtonMapping::UpdatePad(CONTROLLERBUTTONS_T& padButtons) {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return;
    }

    auto normalize = [](float deg) {
        deg = std::fmod(deg, 360.0f);
        if (deg < 0.0f) deg += 360.0f;
        return deg;
    };
    const float start = normalize(mStartAngleDeg);
    const float end = normalize(mEndAngleDeg);

    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        const int nFingers = SDL_GameControllerGetNumTouchpadFingers(gamepad, mTouchpadIndex);
        for (int f = 0; f < nFingers; ++f) {
            Uint8 state = 0;
            float x = 0.0f, y = 0.0f, pressure = 0.0f;
            if (SDL_GameControllerGetTouchpadFinger(gamepad, mTouchpadIndex, f, &state, &x, &y, &pressure) != 0) {
                continue;
            }
            if (!state) {
                continue;
            }

            const float dx = x - 0.5f;
            const float dy = y - 0.5f;
            const float r = std::sqrt(dx * dx + dy * dy);
            if (r < mInnerRadius) {
                continue;
            }

            float angle = std::atan2(dy, dx) * 180.0f / (float)M_PI;
            if (angle < 0.0f) angle += 360.0f;

            bool inWedge;
            if (start <= end) {
                inWedge = (angle >= start && angle < end);
            } else {
                inWedge = (angle >= start || angle < end);
            }
            if (inWedge) {
                padButtons |= mBitmask;
                return;
            }
        }
    }
}

int8_t SDLTouchpadWedgeToButtonMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLTouchpadWedgeToButtonMapping::GetButtonMappingId() {
    return StringHelper::Sprintf("P%d-B%d-SDLTPW%d-A%d_%d-IR%d", mPortIndex, mBitmask, mTouchpadIndex,
                                 (int)std::round(mStartAngleDeg), (int)std::round(mEndAngleDeg),
                                 (int)std::round(mInnerRadius * 1000));
}

void SDLTouchpadWedgeToButtonMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    auto cvars = Ship::Context::GetInstance()->GetConsoleVariables();
    cvars->SetString(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str(),
                     "SDLTouchpadWedgeToButtonMapping");
    cvars->SetInteger(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str(), mBitmask);
    cvars->SetInteger(StringHelper::Sprintf("%s.TouchpadIndex", mappingCvarKey.c_str()).c_str(), mTouchpadIndex);
    cvars->SetFloat(StringHelper::Sprintf("%s.StartAngle", mappingCvarKey.c_str()).c_str(), mStartAngleDeg);
    cvars->SetFloat(StringHelper::Sprintf("%s.EndAngle", mappingCvarKey.c_str()).c_str(), mEndAngleDeg);
    cvars->SetFloat(StringHelper::Sprintf("%s.InnerRadius", mappingCvarKey.c_str()).c_str(), mInnerRadius);
    cvars->Save();
}

void SDLTouchpadWedgeToButtonMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    auto cvars = Ship::Context::GetInstance()->GetConsoleVariables();
    cvars->ClearVariable(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.TouchpadIndex", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.StartAngle", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.EndAngle", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.InnerRadius", mappingCvarKey.c_str()).c_str());
    cvars->Save();
}

std::string SDLTouchpadWedgeToButtonMapping::GetPhysicalDeviceName() {
    return "SDL Gamepad";
}

std::string SDLTouchpadWedgeToButtonMapping::GetPhysicalInputName() {
    return StringHelper::Sprintf("Touchpad %s wedge [%.0f-%.0f deg]", mTouchpadIndex == 0 ? "L" : "R",
                                 mStartAngleDeg, mEndAngleDeg);
}

} // namespace Ship
