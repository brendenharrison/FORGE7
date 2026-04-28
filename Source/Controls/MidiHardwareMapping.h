#pragma once

#include <array>
#include <cstdint>

#include <juce_core/juce_core.h>

namespace forge7
{

/** Default MIDI -> `HardwareControlId` layout for FORGE 7 development builds.

    Edit fields at runtime via `MidiControlInput::setDevelopmentMapping` (message thread) or replace
    this struct later with persisted JSON / hardware profiles.

    **Relative encoder CCs:** any CC listed in `encoderRelativeCcNumbers` (including `ccEncoderPrimary`)
    is interpreted as a signed step: `delta = value - 64` (7-bit center detent). Add MAC/Behringer-style
    secondary CCs here without changing handler code. */
struct DevelopmentMidiMapping
{
    /** Absolute 0...127 -> normalized knob (CC numbers per knob). */
    std::array<int, 4> ccKnobs { { 20, 21, 22, 23 } };

    int noteAssignButton1 { 60 };
    int noteAssignButton2 { 61 };
    int noteChainPrevious { 62 };
    int noteChainNext { 63 };

    /** Encoder rotation: primary CC (also listed in `encoderRelativeCcNumbers`). */
    int ccEncoderPrimary { 24 };

    /** All CC numbers treated as relative encoder (center 64). Always include `ccEncoderPrimary`. */
    juce::Array<int> encoderRelativeCcNumbers { 24 };

    int noteEncoderPress { 64 };

    /** Optional: future map for encoder long-press on a dedicated note (not used in default routing). */
    int noteEncoderLongPress { -1 };

    DevelopmentMidiMapping();

    /** Ensures `encoderRelativeCcNumbers` contains `ccEncoderPrimary` once. */
    void normalizeEncoderCcList();

    bool operator==(const DevelopmentMidiMapping& o) const noexcept;
    bool operator!=(const DevelopmentMidiMapping& o) const noexcept { return !(*this == o); }
};

} // namespace forge7
