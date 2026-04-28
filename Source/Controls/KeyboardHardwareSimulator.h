#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

class ControlManager;

/** Development-only: maps QWERTY keys to `HardwareControlEvent` via `ControlManager::submitHardwareEvent`.

    Attach to the root `Component` and give it keyboard focus — **not** for production; replace with
    USB serial / GPIO when hardware exists.

    Optional **press-and-rotate** can be layered later (track modifier + encoder delta). */
class KeyboardHardwareSimulator final : public juce::KeyListener
{
public:
    explicit KeyboardHardwareSimulator(ControlManager& controlManager);
    ~KeyboardHardwareSimulator() override;

    void attachTo(juce::Component& component);
    void detach();

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void bumpKnob(int knobIndex01, float delta) const;

    ControlManager& controlManager;
    juce::Component* attachedTo = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardHardwareSimulator)
};

} // namespace forge7
