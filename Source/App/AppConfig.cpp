#include "AppConfig.h"

namespace forge7
{
namespace
{
constexpr const char* kConfigDir = "FORGE7";
constexpr const char* kConfigFileName = "forge7_config.json";
} // namespace

AppConfig::AppConfig()
{
    seedDefaults();
}

juce::File AppConfig::getDefaultConfigFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(kConfigDir)
        .getChildFile(kConfigFileName);
}

void AppConfig::seedDefaults()
{
    auto* o = new juce::DynamicObject();
    o->setProperty("projectFileVersion", 1);

    o->setProperty("audioDeviceName", juce::var());
    o->setProperty("audioDeviceType", juce::var());
    o->setProperty("sampleRate", 48000.0);
    o->setProperty("bufferSize", 64);
    o->setProperty("audioInputChannelsMask", 0);
    o->setProperty("audioOutputChannelsMask", 0);
    o->setProperty("audioInputDeviceName", juce::var());
    o->setProperty("audioOutputDeviceName", juce::var());
    o->setProperty("audioDeviceStateXml", juce::var());

    juce::Array<juce::var> scanDirs;
    o->setProperty("pluginScanDirectories", juce::var(scanDirs));

    o->setProperty("defaultProjectPath", juce::var());
    o->setProperty("midiInputDeviceName", juce::var());
    o->setProperty("serialControlDevicePath", juce::var());

    o->setProperty("touchscreenModeEnabled", true);
    o->setProperty("fullscreenModeEnabled", false);
    o->setProperty("bootIntoPerformanceMode", false);
    o->setProperty("lastLoadedProjectPath", juce::var());
    o->setProperty("lastLoadedSetlistPath", juce::var());
    o->setProperty("uiScaleFactor", 1.0);
    o->setProperty("debugLoggingEnabled", false);

    /** When true (and `FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW` is 1), opens DevToolsWindow at startup. */
    o->setProperty("showSimulatedControls", true);

    /** Dev-only: default off - normal Sim HW sends relative deltas for assignable knobs. */
    o->setProperty("simDevAbsoluteKnobTest", false);

    o->setProperty("tunerMutesOutput", true);

    settings = juce::var(o);
}

bool AppConfig::getShowSimulatedControls() const noexcept
{
    if (auto* o = settings.getDynamicObject())
    {
        const juce::var v = o->getProperty("showSimulatedControls");

        if (v.isVoid() || v.isUndefined())
            return true;

        return static_cast<bool>(v);
    }

    return true;
}

bool AppConfig::getSimDevAbsoluteKnobTest() const noexcept
{
    if (auto* o = settings.getDynamicObject())
    {
        const juce::var v = o->getProperty("simDevAbsoluteKnobTest");

        if (v.isVoid() || v.isUndefined())
            return false;

        return static_cast<bool>(v);
    }

    return false;
}

bool AppConfig::loadOrCreateDefaults()
{
    seedDefaults();

    const juce::File f = getDefaultConfigFile();

    if (! f.existsAsFile())
        return true;

    juce::var parsed;

    if (! juce::JSON::parse(f.loadFileAsString(), parsed).wasOk() || ! parsed.isObject())
        return true;

    if (auto* incoming = parsed.getDynamicObject())
        if (auto* base = settings.getDynamicObject())
            for (auto& prop : incoming->getProperties())
                base->setProperty(prop.name, prop.value);

    return true;
}

bool AppConfig::saveToFile() const
{
    const juce::File f = getDefaultConfigFile();

    if (! f.getParentDirectory().createDirectory())
        return false;

    const juce::String json = juce::JSON::toString(settings, false);

    if (json.isEmpty())
        return false;

    return f.replaceWithText(json);
}

juce::String AppConfig::getAudioDeviceStateXml() const
{
    if (auto* o = settings.getDynamicObject())
    {
        const juce::var v = o->getProperty("audioDeviceStateXml");
        if (v.isString())
            return v.toString();
    }

    return {};
}

void AppConfig::setAudioDeviceStateXml(const juce::String& xml)
{
    if (auto* o = settings.getDynamicObject())
        o->setProperty("audioDeviceStateXml", xml);
}

juce::String AppConfig::getLastLoadedProjectPath() const
{
    if (auto* o = settings.getDynamicObject())
        return o->getProperty("lastLoadedProjectPath").toString();

    return {};
}

void AppConfig::setLastLoadedProjectPath(const juce::String& absolutePath)
{
    if (auto* o = settings.getDynamicObject())
        o->setProperty("lastLoadedProjectPath", absolutePath);
}

bool AppConfig::getTunerMutesOutput() const noexcept
{
    if (auto* o = settings.getDynamicObject())
    {
        const juce::var v = o->getProperty("tunerMutesOutput");

        if (v.isVoid() || v.isUndefined())
            return true;

        return static_cast<bool>(v);
    }

    return true;
}

void AppConfig::setTunerMutesOutput(const bool shouldMute) noexcept
{
    if (auto* o = settings.getDynamicObject())
        o->setProperty("tunerMutesOutput", shouldMute);
}

} // namespace forge7
