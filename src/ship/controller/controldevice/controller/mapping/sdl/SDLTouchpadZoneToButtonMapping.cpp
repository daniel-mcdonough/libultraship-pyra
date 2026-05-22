#include "ship/controller/controldevice/controller/mapping/sdl/SDLTouchpadZoneToButtonMapping.h"

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/utils/StringHelper.h"

namespace Ship {

SDLTouchpadZoneToButtonMapping::SDLTouchpadZoneToButtonMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask,
                                                               int32_t touchpadIndex, float xMin, float xMax,
                                                               float yMin, float yMax)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerButtonMapping(PhysicalDeviceType::SDLGamepad, portIndex, bitmask),
      mTouchpadIndex(touchpadIndex), mXMin(xMin), mXMax(xMax), mYMin(yMin), mYMax(yMax) {
}

void SDLTouchpadZoneToButtonMapping::UpdatePad(CONTROLLERBUTTONS_T& padButtons) {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return;
    }

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
            if (state && x >= mXMin && x <= mXMax && y >= mYMin && y <= mYMax) {
                padButtons |= mBitmask;
                return;
            }
        }
    }
}

int8_t SDLTouchpadZoneToButtonMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLTouchpadZoneToButtonMapping::GetButtonMappingId() {
    // ID is stable across runs because zone bounds are quantized to 1/1000 units.
    return StringHelper::Sprintf("P%d-B%d-SDLTP%d-X%d_%d-Y%d_%d", mPortIndex, mBitmask, mTouchpadIndex,
                                 (int)(mXMin * 1000), (int)(mXMax * 1000), (int)(mYMin * 1000), (int)(mYMax * 1000));
}

void SDLTouchpadZoneToButtonMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    auto cvars = Ship::Context::GetInstance()->GetConsoleVariables();
    cvars->SetString(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str(),
                     "SDLTouchpadZoneToButtonMapping");
    cvars->SetInteger(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str(), mBitmask);
    cvars->SetInteger(StringHelper::Sprintf("%s.TouchpadIndex", mappingCvarKey.c_str()).c_str(), mTouchpadIndex);
    cvars->SetFloat(StringHelper::Sprintf("%s.XMin", mappingCvarKey.c_str()).c_str(), mXMin);
    cvars->SetFloat(StringHelper::Sprintf("%s.XMax", mappingCvarKey.c_str()).c_str(), mXMax);
    cvars->SetFloat(StringHelper::Sprintf("%s.YMin", mappingCvarKey.c_str()).c_str(), mYMin);
    cvars->SetFloat(StringHelper::Sprintf("%s.YMax", mappingCvarKey.c_str()).c_str(), mYMax);
    cvars->Save();
}

void SDLTouchpadZoneToButtonMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    auto cvars = Ship::Context::GetInstance()->GetConsoleVariables();
    cvars->ClearVariable(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.TouchpadIndex", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.XMin", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.XMax", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.YMin", mappingCvarKey.c_str()).c_str());
    cvars->ClearVariable(StringHelper::Sprintf("%s.YMax", mappingCvarKey.c_str()).c_str());
    cvars->Save();
}

std::string SDLTouchpadZoneToButtonMapping::GetPhysicalDeviceName() {
    return "SDL Gamepad";
}

std::string SDLTouchpadZoneToButtonMapping::GetPhysicalInputName() {
    return StringHelper::Sprintf("Touchpad %s [%.2f-%.2f, %.2f-%.2f]", mTouchpadIndex == 0 ? "L" : "R", mXMin, mXMax,
                                 mYMin, mYMax);
}

} // namespace Ship
