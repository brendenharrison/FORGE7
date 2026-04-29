#include "ControlManager.h"

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "MidiControlInput.h"

#include "../App/ProjectSession.h"
#include "../Utilities/Logger.h"
#include "ParameterMappingManager.h"

namespace forge7
{
namespace
{
juce::String hardwareControlSourceDebugName(HardwareControlSource source) noexcept
{
    switch (source)
    {
        case HardwareControlSource::Midi:
            return "Midi";
        case HardwareControlSource::SimulatedKeyboard:
            return "SimulatedKeyboard";
        case HardwareControlSource::SimulatedGui:
            return "SimulatedGui";
        case HardwareControlSource::UsbSerial:
            return "UsbSerial";
        case HardwareControlSource::FutureGpio:
            return "FutureGpio";
        default:
            return "Unknown";
    }
}
} // namespace

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

void ControlManager::attachProjectSession(ProjectSession* session) noexcept
{
    projectSession = session;
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
    if (projectSession == nullptr)
        return;

    if (e.type != HardwareControlType::ButtonPressed)
        return;

    if (e.id == HardwareControlId::ChainPreviousButton)
        projectSession->previousChain();
    else if (e.id == HardwareControlId::ChainNextButton)
        projectSession->nextChain();
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

    // Dev-only: trace simulated GUI events (throttled).
    if (event.source == HardwareControlSource::SimulatedGui)
    {
        static int every = 0;
        if ((++every % 12) == 0)
            Logger::info("FORGE7 SimHW: ControlManager received id=" + juce::String((int)event.id)
                         + " type=" + juce::String((int)event.type) + " value=" + juce::String(event.value, 3));
    }

    /** Mapping, scene hydration, and listeners require the message thread - bundle so order is fixed. */
    auto deliver = [this, event]()
    {
        if (event.id == HardwareControlId::EncoderLongPress)
        {
            Logger::info("FORGE7 ControlManager: EncoderLongPress received source="
                         + hardwareControlSourceDebugName(event.source));
        }

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
