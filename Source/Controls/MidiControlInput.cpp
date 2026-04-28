#include "MidiControlInput.h"

#include <juce_core/juce_core.h>

#include "ControlManager.h"
#include "HardwareControlTypes.h"

namespace forge7
{
namespace
{
bool lookupNoteToHardwareId(const DevelopmentMidiMapping& map, const int note, HardwareControlId& outId)
{
    if (note == map.noteAssignButton1)
    {
        outId = HardwareControlId::AssignButton1;
        return true;
    }

    if (note == map.noteAssignButton2)
    {
        outId = HardwareControlId::AssignButton2;
        return true;
    }

    if (note == map.noteChainPrevious)
    {
        outId = HardwareControlId::ChainPreviousButton;
        return true;
    }

    if (note == map.noteChainNext)
    {
        outId = HardwareControlId::ChainNextButton;
        return true;
    }

    if (note == map.noteEncoderPress)
    {
        outId = HardwareControlId::EncoderPress;
        return true;
    }

    if (map.noteEncoderLongPress >= 0 && note == map.noteEncoderLongPress)
    {
        outId = HardwareControlId::EncoderLongPress;
        return true;
    }

    return false;
}
} // namespace

MidiControlInput::MidiControlInput(ControlManager& owner)
    : controlManager(owner)
{
}

MidiControlInput::~MidiControlInput()
{
    stopAndReleaseDevice();
}

juce::Array<juce::MidiDeviceInfo> MidiControlInput::getAvailableInputDevices()
{
    return juce::MidiInput::getAvailableDevices();
}

void MidiControlInput::stopAndReleaseDevice() noexcept
{
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput.reset();
    }
}

void MidiControlInput::attachToAvailableInput()
{
    openDeviceAtIndex(0);
}

bool MidiControlInput::openDeviceAtIndex(const int deviceIndex)
{
    const auto devices = getAvailableInputDevices();

    if (! juce::isPositiveAndBelow(deviceIndex, devices.size()))
        return false;

    return openDeviceWithIdentifier(devices[deviceIndex].identifier);
}

bool MidiControlInput::openDeviceWithIdentifier(const juce::String& deviceIdentifier)
{
    if (deviceIdentifier.isEmpty())
        return false;

    stopAndReleaseDevice();

    midiInput = juce::MidiInput::openDevice(deviceIdentifier, this);

    if (midiInput == nullptr)
        return false;

    midiInput->start();
    return true;
}

void MidiControlInput::closeCurrentDevice()
{
    stopAndReleaseDevice();
}

juce::String MidiControlInput::getCurrentDeviceIdentifier() const
{
    if (midiInput == nullptr)
        return {};

    return midiInput->getIdentifier();
}

int MidiControlInput::getCurrentDeviceIndex() const
{
    const juce::String id = getCurrentDeviceIdentifier();

    if (id.isEmpty())
        return -1;

    const auto devices = getAvailableInputDevices();

    for (int i = 0; i < devices.size(); ++i)
        if (devices.getReference(i).identifier == id)
            return i;

    return -1;
}

DevelopmentMidiMapping MidiControlInput::getDevelopmentMapping() const
{
    const juce::ScopedLock lock(mappingLock);
    return developmentMapping;
}

void MidiControlInput::setDevelopmentMapping(const DevelopmentMidiMapping& mapping)
{
    DevelopmentMidiMapping copy(mapping);
    copy.normalizeEncoderCcList();

    const juce::ScopedLock lock(mappingLock);
    developmentMapping = std::move(copy);
}

float MidiControlInput::ccValueToNormalized(const int value7) noexcept
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(value7) / 127.0f);
}

float MidiControlInput::relativeCcToDelta(const int value7) noexcept
{
    /** Standard 7-bit “relative with center bias”: 64 = no movement. */
    return static_cast<float>(value7 - 64);
}

void MidiControlInput::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    juce::ignoreUnused(source);

    /** Copy mapping under lock — short critical section; no plugin or audio graph access. */
    DevelopmentMidiMapping map;
    {
        const juce::ScopedLock lock(mappingLock);
        map = developmentMapping;
    }

    auto submit = [&](HardwareControlEvent ev)
    {
        ev.source = HardwareControlSource::Midi;
        controlManager.submitHardwareEvent(ev);
    };

    if (message.isController())
    {
        const int cc = message.getControllerNumber();
        const int v = message.getControllerValue();

        for (int i = 0; i < 4; ++i)
        {
            if (cc == map.ccKnobs[static_cast<size_t>(i)])
            {
                HardwareControlEvent ev {};
                ev.id = static_cast<HardwareControlId>(static_cast<int>(HardwareControlId::Knob1) + i);
                ev.type = HardwareControlType::AbsoluteNormalized;
                ev.value = ccValueToNormalized(v);
                submit(ev);
                return;
            }
        }

        for (const int relCc : map.encoderRelativeCcNumbers)
        {
            if (cc == relCc)
            {
                HardwareControlEvent ev {};
                ev.id = HardwareControlId::EncoderRotate;
                ev.type = HardwareControlType::RelativeDelta;
                ev.value = relativeCcToDelta(v);
                submit(ev);
                return;
            }
        }

        return;
    }

    const int note = message.getNoteNumber();

    HardwareControlId noteTarget {};

    if (! lookupNoteToHardwareId(map, note, noteTarget))
        return;

    if (message.isNoteOn())
    {
        const float vel = message.getFloatVelocity();

        if (vel <= 0.0f)
        {
            HardwareControlEvent ev {};
            ev.id = noteTarget;
            ev.type = HardwareControlType::ButtonReleased;
            ev.value = 0.0f;
            submit(ev);
            return;
        }

        HardwareControlEvent ev {};
        ev.id = noteTarget;
        ev.type = HardwareControlType::ButtonPressed;
        ev.value = 0.0f;
        submit(ev);
        return;
    }

    if (message.isNoteOff())
    {
        HardwareControlEvent ev {};
        ev.id = noteTarget;
        ev.type = HardwareControlType::ButtonReleased;
        ev.value = 0.0f;
        submit(ev);
    }
}

} // namespace forge7
