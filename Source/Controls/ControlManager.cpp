#include "ControlManager.h"

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "MidiControlInput.h"

#include "../PluginHost/PluginHostManager.h"
#include "../Scene/SceneManager.h"
#include "ParameterMappingManager.h"

namespace forge7
{

ControlManager::ControlManager(ParameterMappingManager& mappingManager)
    : parameterMappingManager(mappingManager)
    , midiInput(std::make_unique<MidiControlInput>(*this))
{
    if (midiInput != nullptr)
        midiInput->attachToAvailableInput();
}

ControlManager::~ControlManager() = default;

void ControlManager::addListener(Listener* listener)
{
    if (listener == nullptr)
        return;

    const juce::ScopedLock lock(controlLock);
    listeners.add(listener);
}

void ControlManager::removeListener(Listener* listener)
{
    if (listener == nullptr)
        return;

    const juce::ScopedLock lock(controlLock);
    listeners.remove(listener);
}

void ControlManager::attachSceneNavigation(SceneManager* scenes, PluginHostManager* host) noexcept
{
    sceneManager = scenes;
    pluginHostManager = host;
}

void ControlManager::applyEventToState(const HardwareControlEvent& e)
{
    switch (e.type)
    {
        case HardwareControlType::AbsoluteNormalized:
            if (const int k = knobIndexFromId(e.id); k >= 0)
                hardwareState.applyAbsoluteKnob(k, e.value);
            break;

        case HardwareControlType::RelativeDelta:
            if (e.id == HardwareControlId::EncoderRotate)
            {
                hardwareState.addEncoderDetents(juce::roundToInt(e.value));
            }
            break;

        case HardwareControlType::ButtonPressed:
        case HardwareControlType::ButtonReleased:
        {
            const bool down = (e.type == HardwareControlType::ButtonPressed);

            if (e.id == HardwareControlId::AssignButton1)
                hardwareState.setAssignButton(0, down);
            else if (e.id == HardwareControlId::AssignButton2)
                hardwareState.setAssignButton(1, down);
            else if (e.id == HardwareControlId::ChainPreviousButton)
                hardwareState.setChainPrevious(down);
            else if (e.id == HardwareControlId::ChainNextButton)
                hardwareState.setChainNext(down);
            break;
        }

        default:
            break;
    }
}

void ControlManager::routeThroughMappingStub(const HardwareControlEvent& e)
{
    /** Knobs + assign buttons only - chain/navigation handled elsewhere (`invokeSceneNavigationIfAttached`). */
    if (e.type == HardwareControlType::AbsoluteNormalized && isKnobId(e.id))
    {
        parameterMappingManager.processHardwareEvent(e);
        return;
    }

    if (isAssignButtonId(e.id)
        && (e.type == HardwareControlType::ButtonPressed || e.type == HardwareControlType::ButtonReleased))
        parameterMappingManager.processHardwareEvent(e);
}

void ControlManager::invokeSceneNavigationIfAttached(const HardwareControlEvent& e)
{
    if (sceneManager == nullptr || pluginHostManager == nullptr)
        return;

    if (e.type != HardwareControlType::ButtonPressed)
        return;

    if (e.id == HardwareControlId::ChainPreviousButton)
        sceneManager->previousChainVariationWithCrossfade(*pluginHostManager);
    else if (e.id == HardwareControlId::ChainNextButton)
        sceneManager->nextChainVariationWithCrossfade(*pluginHostManager);
}

void ControlManager::notifyListeners(const HardwareControlEvent& e)
{
    const juce::ScopedLock lock(controlLock);

    listeners.call([&](Listener& l)
                   {
                       if (e.type == HardwareControlType::AbsoluteNormalized && isKnobId(e.id))
                           l.parameterControlChanged(e.id, e.value);

                       if (isAssignButtonId(e.id)
                           && (e.type == HardwareControlType::ButtonPressed
                               || e.type == HardwareControlType::ButtonReleased))
                           l.assignableButtonChanged(e.id, e.type == HardwareControlType::ButtonPressed);

                       if (e.id == HardwareControlId::ChainPreviousButton && e.type == HardwareControlType::ButtonPressed)
                           l.chainVariationPreviousPressed();

                       if (e.id == HardwareControlId::ChainNextButton && e.type == HardwareControlType::ButtonPressed)
                           l.chainVariationNextPressed();

                       if (isEncoderLogicalId(e.id))
                           l.encoderNavigationEvent(e);
                   });
}

void ControlManager::submitHardwareEvent(HardwareControlEvent event)
{
    if (event.timestampSeconds <= 0.0 && juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        event.timestampSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // Always safe: atomics only, safe from MIDI / serial threads.
    applyEventToState(event);

    /** Mapping, scene hydration, and listeners require the message thread - bundle so order is fixed. */
    auto deliver = [this, event]()
    {
        routeThroughMappingStub(event);
        invokeSceneNavigationIfAttached(event);
        notifyListeners(event);
    };

    if (juce::MessageManager::getInstanceWithoutCreating() == nullptr)
    {
        deliver();
        return;
    }

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        deliver();
        return;
    }

    juce::MessageManager::callAsync(std::move(deliver));
}

} // namespace forge7
