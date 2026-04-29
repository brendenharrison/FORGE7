#include "ForgeStoragePaths.h"

namespace forge7
{
namespace
{

juce::File libraryRootOnThisOs()
{
#if JUCE_LINUX
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".local")
        .getChildFile("share")
        .getChildFile("forge7");
#else
    /** macOS, Windows, and other desktops: ~/Documents/FORGE7 */
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("FORGE7");
#endif
}

} // namespace

juce::File getForgeUserDataDirectory()
{
    return libraryRootOnThisOs();
}

juce::File getProjectsDirectory()
{
    return getForgeUserDataDirectory().getChildFile("Projects");
}

juce::File getScenesDirectory()
{
    return getForgeUserDataDirectory().getChildFile("Scenes");
}

juce::File getPresetsDirectory()
{
    return getForgeUserDataDirectory().getChildFile("Presets");
}

juce::File getPluginCacheDirectory()
{
    return getForgeUserDataDirectory().getChildFile("PluginCache");
}

juce::File getBackupsDirectory()
{
    return getForgeUserDataDirectory().getChildFile("Backups");
}

bool ensureForgeStorageFoldersExist()
{
    bool ok = true;
    auto mkdir = [&ok](const juce::File& d)
    {
        const juce::Result r = d.createDirectory();
        ok = ok && r.wasOk();
    };

    mkdir(getForgeUserDataDirectory());
    mkdir(getProjectsDirectory());
    mkdir(getScenesDirectory());
    mkdir(getPresetsDirectory());
    mkdir(getPluginCacheDirectory());
    mkdir(getBackupsDirectory());
    return ok;
}

juce::String sanitizeLibraryItemName(const juce::String& userName)
{
    const juce::String trimmed = userName.trim();

    if (trimmed.isEmpty())
        return "Untitled";

    juce::String out;
    bool lastWasSpace = false;

    for (int i = 0; i < trimmed.length(); ++i)
    {
        const juce::juce_wchar c = trimmed[i];

        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>'
            || c == '|' || c == ';' || c == 0)
        {
            if (! lastWasSpace)
            {
                out += '-';
                lastWasSpace = true;
            }
        }
        else if (c == '\t' || c == '\n' || c == '\r' || c == ' ')
        {
            if (! lastWasSpace)
            {
                out += ' ';
                lastWasSpace = true;
            }
        }
        else
        {
            out += c;
            lastWasSpace = false;
        }
    }

    juce::String s = out.trim();
    while (s.endsWithChar('.'))
        s = s.substring(0, s.length() - 1);

    s = s.trim();
    if (s.isEmpty())
        return "Untitled";

    return s;
}

juce::Array<juce::File> listLibraryProjectFiles()
{
    ensureForgeStorageFoldersExist();

    juce::Array<juce::File> found;
    getProjectsDirectory().findChildFiles(found, juce::File::findFiles, false, juce::String("*") + kForgeProjectExtension);

    std::sort(found.begin(), found.end(), [](const juce::File& a, const juce::File& b)
              {
                  return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
              });

    return found;
}

} // namespace forge7
