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
    o->setProperty("sampleRate", 48000.0);
    o->setProperty("bufferSize", 64);

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

} // namespace forge7
