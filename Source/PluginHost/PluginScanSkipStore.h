#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

/** Persists VST3 bundles to skip during **in-process** scanning.

    Entries store the bundle path and **last modification time** (from the filesystem). A skip applies
    only while that revision is on disk — if the vendor ships an update (different mod time), the entry
    is dropped automatically on the next scan so the new build is tried again.

    Crash handling: immediately before probing a bundle we write `scan_pending_probe.json`; after a
    successful probe we delete it. If the host crashes during load, the next launch treats that path +
    mod time as a temporary skip (`reason`: `scan_crash`) instead of permanently blacklisting via JUCE. */
class PluginScanSkipStore
{
public:
    PluginScanSkipStore() = default;

    bool load();
    bool save() const;

    /** Drops entries whose bundle is missing or has a newer mod time (plugin updated — retry scan). */
    void pruneStaleEntries();

    bool shouldSkipScanning(const juce::String& bundlePath) const;

    void recordSkip(const juce::String& bundlePath, juce::int64 modTimeMs, const juce::String& reason);

    void clearAllEntries();

    void writePendingProbe(const juce::String& bundlePath, juce::int64 modTimeMs) const;
    void clearPendingProbe() const;

    juce::File getSkipsFile() const;
    juce::File getPendingProbeFile() const;

    /** After a crash mid-scan, convert pending probe file into a mod-time skip entry. */
    void recoverFromCrashProbeFile();

private:
    struct Entry
    {
        juce::String path;
        juce::int64 modTimeMs { 0 };
        juce::String reason;
    };

    juce::Array<Entry> entries;
};

} // namespace forge7
