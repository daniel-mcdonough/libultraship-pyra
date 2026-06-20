#pragma once

#include <cstdint>
#include <string>

#include "ship/controller/controldevice/controller/mapping/ControllerAxisDirectionMapping.h"
#include "ship/controller/controldevice/controller/mapping/ControllerInputMapping.h"
#include "ship/controller/controldevice/controller/mapping/sdl/SDLMapping.h"

namespace Ship {

/**
 * @brief Maps one half of one axis on an SDL gamepad touchpad to a virtual stick direction.
 *
 * The touchpad surface is treated as a square in [0,1] x [0,1] with the center at (0.5, 0.5).
 * The configured axis (X or Y) and half (POSITIVE = right/down side, NEGATIVE = left/up side)
 * select which fingers contribute. Distance from center is rescaled past the deadzone and
 * returned in 0..MAX_AXIS_RANGE for the framework to combine with other mappings on the same
 * stick + direction.
 */
class SDLTouchpadToAxisDirectionMapping final : public ControllerAxisDirectionMapping {
  public:
    SDLTouchpadToAxisDirectionMapping(uint8_t portIndex, StickIndex stickIndex, Direction direction,
                                      int32_t touchpadIndex, int32_t axis, int32_t axisDirection, float deadzone,
                                      float sensitivity);

    float GetNormalizedAxisDirectionValue() override;
    int8_t GetMappingType() override;
    std::string GetAxisDirectionMappingId() override;

    void SaveToConfig() override;
    void EraseFromConfig() override;

    std::string GetPhysicalDeviceName() override;
    std::string GetPhysicalInputName() override;

    int32_t GetTouchpadIndex() const { return mTouchpadIndex; }
    int32_t GetAxis() const { return mAxis; }
    int32_t GetAxisDirection() const { return mAxisDirection; }
    float GetDeadzone() const { return mDeadzone; }
    float GetSensitivity() const { return mSensitivity; }

  private:
    int32_t mTouchpadIndex;
    int32_t mAxis;          // Axis enum: X = 0, Y = 1
    int32_t mAxisDirection; // AxisDirection enum: NEGATIVE = -1, POSITIVE = +1
    float mDeadzone;        // 0..1, distance from center below which input is ignored
    float mSensitivity;     // multiplier applied post-deadzone (1.0 = linear; >1 amplifies, <1 attenuates)
};

} // namespace Ship
