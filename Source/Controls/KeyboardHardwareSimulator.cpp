#include "KeyboardHardwareSimulator.h"

#include <juce_core/juce_core.h>

#include "ControlManager.h"
#include "HardwareControlTypes.h"

namespace forge7
{

KeyboardHardwareSimulator::KeyboardHardwareSimulator(ControlManager& cm)
    : controlManager(cm)
{
}

KeyboardHardwareSimulator::~KeyboardHardwareSimulator()
{
    detach();
}

void KeyboardHardwareSimulator::attachTo(juce::Component& component)
{
    detach();
    attachedTo = &component;
    attachedTo->addKeyListener(this);
}

void KeyboardHardwareSimulator::detach()
{
    if (attachedTo != nullptr)
    {
        attachedTo->removeKeyListener(this);
        attachedTo = nullptr;
    }
}

void KeyboardHardwareSimulator::bumpKnob(const int knobIndex01, const float delta) const
{
    HardwareControlEvent e {};
    e.id = static_cast<HardwareControlId>(static_cast<int>(HardwareControlId::Knob1) + knobIndex01);
    e.type = HardwareControlType::RelativeDelta;
    e.source = HardwareControlSource::SimulatedKeyboard;
    e.value = juce::jlimit(-0.25f, 0.25f, delta);
    controlManager.submitHardwareEvent(e);
}

bool KeyboardHardwareSimulator::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    auto sendButton = [&](const HardwareControlId id, const HardwareControlType type)
    {
        HardwareControlEvent e {};
        e.id = id;
        e.type = type;
        e.source = HardwareControlSource::SimulatedKeyboard;
        controlManager.submitHardwareEvent(e);
    };

    const auto kc = key.getKeyCode();

    if (kc == 'q')
    {
        bumpKnob(0, 0.05f);
        return true;
    }

    if (kc == 'w')
    {
        bumpKnob(1, 0.05f);
        return true;
    }

    if (kc == 'e')
    {
        bumpKnob(2, 0.05f);
        return true;
    }

    if (kc == 'r')
    {
        bumpKnob(3, 0.05f);
        return true;
    }

    if (kc == 'Q')
    {
        bumpKnob(0, -0.05f);
        return true;
    }

    if (kc == 'W')
    {
        bumpKnob(1, -0.05f);
        return true;
    }

    if (kc == 'E')
    {
        bumpKnob(2, -0.05f);
        return true;
    }

    if (kc == 'R')
    {
        bumpKnob(3, -0.05f);
        return true;
    }

    if (kc == 'z')
    {
        sendButton(HardwareControlId::ChainPreviousButton, HardwareControlType::ButtonPressed);
        return true;
    }

    if (kc == 'x')
    {
        sendButton(HardwareControlId::ChainNextButton, HardwareControlType::ButtonPressed);
        return true;
    }

    if (kc == 'a')
    {
        sendButton(HardwareControlId::AssignButton1, HardwareControlType::ButtonPressed);
        return true;
    }

    if (kc == 's')
    {
        sendButton(HardwareControlId::AssignButton2, HardwareControlType::ButtonPressed);
        return true;
    }

    if (kc == '[')
    {
        HardwareControlEvent e {};
        e.id = HardwareControlId::EncoderRotate;
        e.type = HardwareControlType::RelativeDelta;
        e.source = HardwareControlSource::SimulatedKeyboard;
        e.value = -1.0f;
        controlManager.submitHardwareEvent(e);
        return true;
    }

    if (kc == ']')
    {
        HardwareControlEvent e {};
        e.id = HardwareControlId::EncoderRotate;
        e.type = HardwareControlType::RelativeDelta;
        e.source = HardwareControlSource::SimulatedKeyboard;
        e.value = 1.0f;
        controlManager.submitHardwareEvent(e);
        return true;
    }

    if (kc == ' ')
    {
        sendButton(HardwareControlId::EncoderPress, HardwareControlType::ButtonPressed);
        return true;
    }

    if (kc == juce::KeyPress::returnKey || kc == '\r')
    {
        sendButton(HardwareControlId::EncoderLongPress, HardwareControlType::ButtonPressed);
        return true;
    }

    return false;
}

} // namespace forge7
