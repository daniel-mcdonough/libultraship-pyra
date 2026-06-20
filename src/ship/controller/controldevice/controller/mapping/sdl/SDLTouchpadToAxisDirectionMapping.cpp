#include "ship/controller/controldevice/controller/mapping/sdl/SDLTouchpadToAxisDirectionMapping.h"

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/utils/StringHelper.h"

namespace Ship {

SDLTouchpadToAxisDirectionMapping::SDLTouchpadToAxisDirectionMapping(uint8_t portIndex, StickIndex stickIndex,
                                                                     Direction direction, int32_t touchpadIndex,
                                                                     int32_t axis, int32_t axisDirection,
                                                                     float deadzone, float sensitivity)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerAxisDirectionMapping(PhysicalDeviceType::SDLGamepad, portIndex, stickIndex, direction),
      mTouchpadIndex(touchpadIndex), mAxis(axis), mAxisDirection(axisDirection), mDeadzone(deadzone),
      mSensitivity(sensitivity > 0.0f ? sensitivity : 1.0f) {
}

float SDLTouchpadToAxisDirectionMapping::GetNormalizedAxisDirectionValue() {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return 0.0f;
    }

    const float deadzone = std::clamp(mDeadzone, 0.0f, 0.95f);
    float best = 0.0f;

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

            const float coord = (mAxis == X) ? x : y;
            const float offset = coord - 0.5f; // -0.5 .. +0.5

            if ((mAxisDirection == POSITIVE && offset <= 0.0f) ||
                (mAxisDirection == NEGATIVE && offset >= 0.0f)) {
                continue;
            }

            float magnitude = std::fabs(offset) * 2.0f; // 0..1
            if (magnitude <= deadzone) {
                continue;
            }
            magnitude = (magnitude - deadzone) / (1.0f - deadzone); // rescale post-deadzone to 0..1
            magnitude *= mSensitivity;
            magnitude = std::clamp(magnitude, 0.0f, 1.0f);

            const float scaled = magnitude * MAX_AXIS_RANGE;
            if (scaled > best) {
                best = scaled;
            }
        }
    }

    return best;
}

int8_t SDLTouchpadToAxisDirectionMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLTouchpadToAxisDirectionMapping::GetAxisDirectionMappingId() {
    return StringHelper::Sprintf("P%d-S%d-D%d-SDLTP%d-%c%s", mPortIndex, mStickIndex, mDirection, mTouchpadIndex,
                                 mAxis == X ? 'X' : 'Y', mAxisDirection == POSITIVE ? "P" : "N");
}

void SDLTouchpadToAxisDirectionMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    auto cvars = Ship::Context::GetInstance()->GetConsoleVariables();
    cvars->SetString(StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str(),
                     "SDLTouchpadToAxisDirectionMapping");
    cvars->SetInteger(StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str(), mStickIndex);
    cvars->SetInteger(StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str(), mDirection);
    cvars->SetInteger(StringHelper::Sprintf("%s.TouchpadIndex", mappingCvarKey.c_str()).c_str(), mTouchpadIndex);
    cvars->SetInteger(StringHelper::Sprintf("%s.Axis", mappingCvarKey.c_str()).c_str(), mAxis);
    cvars->SetInteger(StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str(), mAxisDirection);
    cvars->SetFloat(StringHelper::Sprintf("%s.Deadzone", mappingCvarKey.c_str()).c_str(), mDeadzone);
    cvars->SetFloat(StringHelper::Sprintf("%s.Sensitivity", mappingCvarKey.c_str()).c_str(), mSensitivity);
    cvars->Save();
}

void SDLTouchpadToAxisDirectionMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    auto cvars = Ship::Context::GetInstance()->GetConsoleVariables();
    cvars->ClearVariable(StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.TouchpadIndex", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.Axis", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.Deadzone", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.Sensitivity", mappingCvarKey.c_str()).c_str());
    cvars->Save();
}

std::string SDLTouchpadToAxisDirectionMapping::GetPhysicalDeviceName() {
    return "SDL Gamepad";
}

std::string SDLTouchpadToAxisDirectionMapping::GetPhysicalInputName() {
    return StringHelper::Sprintf("Touchpad %s %c%s", mTouchpadIndex == 0 ? "L" : "R", mAxis == X ? 'X' : 'Y',
                                 mAxisDirection == POSITIVE ? "+" : "-");
}

} // namespace Ship
