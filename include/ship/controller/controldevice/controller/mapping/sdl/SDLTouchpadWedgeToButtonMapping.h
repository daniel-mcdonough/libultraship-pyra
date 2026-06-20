#pragma once

#include <cstdint>
#include <string>

#include "ship/controller/controldevice/controller/mapping/ControllerButtonMapping.h"
#include "ship/controller/controldevice/controller/mapping/ControllerInputMapping.h"

namespace Ship {

/**
 * @brief Maps a polar wedge (pie slice) on an SDL gamepad touchpad to a virtual controller button.
 *
 * The touchpad is treated as a unit disc centered at (0.5, 0.5). A finger fires the button
 * when its angle from center is in [startAngleDeg, endAngleDeg) and its distance from center
 * is at least innerRadius. Angles are in degrees with 0 = right, 90 = down, 180 = left, 270 = up.
 * If startAngleDeg > endAngleDeg, the range is treated as wrapping past 360°.
 */
class SDLTouchpadWedgeToButtonMapping final : public ControllerButtonMapping {
  public:
    SDLTouchpadWedgeToButtonMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask, int32_t touchpadIndex,
                                    float startAngleDeg, float endAngleDeg, float innerRadius);

    void UpdatePad(CONTROLLERBUTTONS_T& padButtons) override;

    int8_t GetMappingType() override;
    std::string GetButtonMappingId() override;

    void SaveToConfig() override;
    void EraseFromConfig() override;

    std::string GetPhysicalDeviceName() override;
    std::string GetPhysicalInputName() override;

    int32_t GetTouchpadIndex() const { return mTouchpadIndex; }
    float GetStartAngle() const { return mStartAngleDeg; }
    float GetEndAngle() const { return mEndAngleDeg; }
    float GetInnerRadius() const { return mInnerRadius; }

  private:
    int32_t mTouchpadIndex;
    float mStartAngleDeg;
    float mEndAngleDeg;
    float mInnerRadius;
};

} // namespace Ship
