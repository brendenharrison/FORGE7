#include "PluginScanSkipStore.h"

namespace forge7
{
namespace
{
constexpr const char* kAppFolder = "FORGE7";
constexpr const char* kSkipsFileName = "plugin_scan_skips.json";
constexpr const char* kPendingProbeName = "scan_pending_probe.json";
constexpr int kJsonFormatVersion = 1;

juce::File appSupportSubdir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(kAppFolder);
}

juce::int64 modTimeMsForBundle(const juce::File& bundle)
{
    return static_cast<juce::int64>(bundle.getLastModificationTime().toMilliseconds());
}

} // namespace

juce::File PluginScanSkipStore::getSkipsFile() const
{
    return appSupportSubdir().getChildFile(kSkipsFileName);
}

juce::File PluginScanSkipStore::getPendingProbeFile() const
{
    return appSupportSubdir().getChildFile(kPendingProbeName);
}

bool PluginScanSkipStore::load()
{
    entries.clear();

    const juce::File f = getSkipsFile();

    if (!f.existsAsFile())
        return true;

    juce::var parsed;

    if (!juce::JSON::parse(f.loadFileAsString(), parsed).wasOk() || !parsed.isObject())
        return false;

    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return false;

    const juce::var entriesVar = root->getProperty("entries");

    if (entriesVar.isArray())
    {
        const auto* arr = entriesVar.getArray();

        if (arr != nullptr)
        {
            for (const auto& item : *arr)
            {
                if (auto* o = item.getDynamicObject())
                {
                    Entry e;
                    e.path = o->getProperty("path").toString();
                    e.modTimeMs = static_cast<juce::int64>(static_cast<double>(o->getProperty("modTimeMs")));
                    e.reason = o->getProperty("reason").toString();

                    if (e.path.isNotEmpty())
                        entries.add(std::move(e));
                }
            }
        }
    }

    return true;
}

bool PluginScanSkipStore::save() const
{
    const juce::File dir = appSupportSubdir();

    if (!dir.createDirectory())
        return false;

    juce::Array<juce::var> arr;

    for (const auto& e : entries)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("path", e.path);
        o->setProperty("modTimeMs", static_cast<double>(e.modTimeMs));
        o->setProperty("reason", e.reason.isNotEmpty() ? juce::var(e.reason) : juce::var("scan_skip"));
        arr.add(juce::var(o));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("format", kJsonFormatVersion);
    root->setProperty("entries", juce::var(arr));

    const juce::String json = juce::JSON::toString(juce::var(root), true);

    if (json.isEmpty())
        return false;

    return getSkipsFile().replaceWithText(json);
}

void PluginScanSkipStore::pruneStaleEntries()
{
    for (int i = entries.size(); --i >= 0;)
    {
        const juce::File bundle(entries.getReference(i).path);

        if (!bundle.exists())
        {
            entries.remove(i);
            continue;
        }

        const auto current = modTimeMsForBundle(bundle);

        if (current != entries.getReference(i).modTimeMs)
            entries.remove(i);
    }
}

void PluginScanSkipStore::recoverFromCrashProbeFile()
{
    const juce::File pending = getPendingProbeFile();

    if (!pending.existsAsFile())
        return;

    juce::var parsed;

    if (!juce::JSON::parse(pending.loadFileAsString(), parsed).wasOk() || !parsed.isObject())
    {
        pending.deleteFile();
        return;
    }

    auto* o = parsed.getDynamicObject();

    if (o == nullptr)
    {
        pending.deleteFile();
        return;
    }

    const juce::String path = o->getProperty("path").toString();
    const auto modMs =
        static_cast<juce::int64>(static_cast<double>(o->getProperty("modTimeMs")));

    pending.deleteFile();

    if (path.isEmpty())
        return;

    const juce::File bundle(path);

    if (!bundle.exists())
        return;

    if (modTimeMsForBundle(bundle) != modMs)
        return;

    recordSkip(path, modMs, "scan_crash");
    save();
}

bool PluginScanSkipStore::shouldSkipScanning(const juce::String& bundlePath) const
{
    const juce::File bundle(bundlePath);

    if (!bundle.exists())
        return false;

    const auto current = modTimeMsForBundle(bundle);

    for (const auto& e : entries)
    {
        if (e.path == bundlePath && e.modTimeMs == current)
            return true;
    }

    return false;
}

void PluginScanSkipStore::recordSkip(const juce::String& bundlePath,
                                     const juce::int64 modTimeMs,
                                     const juce::String& reason)
{
    for (int i = entries.size(); --i >= 0;)
        if (entries.getReference(i).path == bundlePath)
            entries.remove(i);

    Entry e;
    e.path = bundlePath;
    e.modTimeMs = modTimeMs;
    e.reason = reason;
    entries.add(std::move(e));
}

void PluginScanSkipStore::clearAllEntries()
{
    entries.clear();
}

void PluginScanSkipStore::writePendingProbe(const juce::String& bundlePath, const juce::int64 modTimeMs) const
{
    const juce::File dir = appSupportSubdir();

    if (!dir.createDirectory())
        return;

    auto* o = new juce::DynamicObject();
    o->setProperty("path", bundlePath);
    o->setProperty("modTimeMs", static_cast<double>(modTimeMs));

    const juce::String json = juce::JSON::toString(juce::var(o), false);
    getPendingProbeFile().replaceWithText(json);
}

void PluginScanSkipStore::clearPendingProbe() const
{
    getPendingProbeFile().deleteFile();
}

} // namespace forge7
