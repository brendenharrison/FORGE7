#pragma once

#include <cstdint>

namespace forge7
{

/** Origin of a normalized control event - **not** required for routing logic; GUI/audio should
    ignore this and handle only `HardwareControlId` + `HardwareControlType`. Useful for diagnostics
    and device-specific calibration (MIDI learn, serial protocol version). */
enum class HardwareControlSource : uint8_t
{
    Unknown = 0,
    Midi,
    SimulatedKeyboard,
    /** macOS/desktop SimulatedControlsComponent / dev panel - diagnostics only. */
    SimulatedGui,
    UsbSerial,
    FutureGpio,
};

/** Stable logical IDs for FORGE 7 fixed hardware (see product layout: K1-K4, assigns, chain, encoder). */
enum class HardwareControlId : uint32_t
{
    Knob1,
    Knob2,
    Knob3,
    Knob4,
    AssignButton1,
    AssignButton2,
    ChainPreviousButton,
    ChainNextButton,
    EncoderRotate,
    EncoderPress,
    EncoderLongPress,
};

/** Shape of the payload - interpretation of `HardwareControlEvent::value` depends on this + `id`. */
enum class HardwareControlType : uint8_t
{
    /** Legacy / dev: absolute 0...1 (e.g. MIDI CC as pot). Prefer `RelativeDelta` for K1-K4. */
    AbsoluteNormalized,

    /** Relative movement.
        - `EncoderRotate`: `value` = signed detents (fractional allowed).
        - `Knob1`...`Knob4`: `value` = signed delta in **plugin normalized 0...1 space** per event
          (typical fine step 0.01f). */
    RelativeDelta,

    /** Momentary buttons: assign, chain, encoder (when mapped as button down/up). */
    ButtonPressed,
    ButtonReleased,
};

/** Single normalized message from any transport (MIDI, USB serial, keyboard dev harness, future GPIO).

    Threading: created on the thread that received raw data; passed to `ControlManager::submitHardwareEvent`.
    Listeners are invoked on the **message thread** (see `ControlManager`). */
struct HardwareControlEvent
{
    HardwareControlId id { HardwareControlId::Knob1 };
    HardwareControlType type { HardwareControlType::AbsoluteNormalized };
    HardwareControlSource source { HardwareControlSource::Unknown };

    /** Meaning depends on `type` / `id`: normalized 0...1, button N/A, or signed delta for encoder. */
    float value { 0.0f };

    /** Optional monotonic stamp (e.g. `Time::getMillisecondCounterHiRes()`); 0 if unknown. */
    double timestampSeconds { 0.0 };
};

constexpr bool isKnobId(HardwareControlId id) noexcept
{
    return id == HardwareControlId::Knob1 || id == HardwareControlId::Knob2 || id == HardwareControlId::Knob3
           || id == HardwareControlId::Knob4;
}

constexpr bool isAssignButtonId(HardwareControlId id) noexcept
{
    return id == HardwareControlId::AssignButton1 || id == HardwareControlId::AssignButton2;
}

constexpr bool isEncoderLogicalId(HardwareControlId id) noexcept
{
    return id == HardwareControlId::EncoderRotate || id == HardwareControlId::EncoderPress
           || id == HardwareControlId::EncoderLongPress;
}

constexpr int knobIndexFromId(HardwareControlId id) noexcept
{
    switch (id)
    {
        case HardwareControlId::Knob1:
            return 0;
        case HardwareControlId::Knob2:
            return 1;
        case HardwareControlId::Knob3:
            return 2;
        case HardwareControlId::Knob4:
            return 3;
        default:
            return -1;
    }
}

} // namespace forge7
