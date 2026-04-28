#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

/** Persistent app settings (`forge7_config.json`). Message thread only.

    Fields mirror the product spec; load/save expanded incrementally. */
class AppConfig
{
public:
    AppConfig();

    /** Default: user app data / FORGE7 / forge7_config.json */
    static juce::File getDefaultConfigFile();

    /** Merge file into internal `settings` var; creates defaults if missing or invalid. */
    bool loadOrCreateDefaults();

    /** Write current `settings` to disk; returns false on I/O error. */
    bool saveToFile() const;

    juce::var& getSettings() noexcept { return settings; }
    const juce::var& getSettings() const noexcept { return settings; }

    /** Dev-only floating simulated hardware panel (default true for desktop iteration). */
    bool getShowSimulatedControls() const noexcept;

private:
    juce::var settings;

    void seedDefaults();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppConfig)
};

} // namespace forge7
