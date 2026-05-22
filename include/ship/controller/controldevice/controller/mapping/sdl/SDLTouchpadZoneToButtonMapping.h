#pragma once

#include <cstdint>
#include <string>

#include "ship/controller/controldevice/controller/mapping/ControllerButtonMapping.h"
#include "ship/controller/controldevice/controller/mapping/ControllerInputMapping.h"

namespace Ship {

/**
 * @brief Maps a rectangular zone on an SDL gamepad touchpad to a virtual controller button.
 *
 * While any finger on the configured touchpad is inside the [xMin, xMax] × [yMin, yMax]
 * region (normalized 0.0–1.0), the corresponding button bitmask bit is set in the pad state.
 * Multiple zones on the same touchpad can be bound independently.
 */
class SDLTouchpadZoneToButtonMapping final : public ControllerButtonMapping {
  public:
    SDLTouchpadZoneToButtonMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask, int32_t touchpadIndex,
                                   float xMin, float xMax, float yMin, float yMax);

    void UpdatePad(CONTROLLERBUTTONS_T& padButtons) override;

    int8_t GetMappingType() override;
    std::string GetButtonMappingId() override;

    void SaveToConfig() override;
    void EraseFromConfig() override;

    std::string GetPhysicalDeviceName() override;
    std::string GetPhysicalInputName() override;

  private:
    int32_t mTouchpadIndex;
    float mXMin;
    float mXMax;
    float mYMin;
    float mYMax;
};

} // namespace Ship
