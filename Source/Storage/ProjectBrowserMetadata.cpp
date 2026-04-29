#include "ProjectBrowserMetadata.h"

namespace forge7
{
namespace
{

juce::String readChainName(juce::DynamicObject* chainObj)
{
    if (chainObj == nullptr)
        return {};

    juce::String n = chainObj->getProperty("chainName").toString();

    if (n.isEmpty())
        n = chainObj->getProperty("variationName").toString();

    return n;
}

} // namespace

bool loadProjectBrowserMetadata(const juce::File& file, ProjectBrowserProjectInfo& out)
{
    out = {};
    out.projectFile = file;

    if (! file.existsAsFile())
        return false;

    juce::String jsonText;

    try
    {
        jsonText = file.loadFileAsString();
    }
    catch (...)
    {
        return false;
    }

    juce::var parsed;

    if (juce::JSON::parse(jsonText, parsed).failed())
        return false;

    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return false;

    out.projectName = root->getProperty("projectName").toString();

    if (out.projectName.isEmpty())
        out.projectName = file.getFileNameWithoutExtension();

    out.activeSceneIndexFromFile = static_cast<int>(root->getProperty("activeSceneIndex"));

    juce::var scenesVar(root->getProperty("scenes"));

    if (! scenesVar.isArray())
        return true;

    const auto* arr = scenesVar.getArray();

    if (arr == nullptr)
        return true;

    int si = 0;

    for (const auto& sv : *arr)
    {
        auto* so = sv.getDynamicObject();

        if (so == nullptr)
        {
            ++si;
            continue;
        }

        ProjectBrowserSceneInfo sc;
        sc.sceneIndex = si;
        sc.sceneName = so->getProperty("sceneName").toString();

        if (sc.sceneName.isEmpty())
            sc.sceneName = so->getProperty("name").toString();

        juce::var chainsVar(so->getProperty("chains"));

        if (! chainsVar.isArray())
            chainsVar = so->getProperty("chainVariations");

        if (chainsVar.isArray())
        {
            const auto* carr = chainsVar.getArray();

            if (carr != nullptr)
            {
                int ci = 0;

                for (const auto& cv : *carr)
                {
                    ProjectBrowserChainInfo ch;
                    ch.chainIndex = ci;
                    ch.chainName = readChainName(cv.getDynamicObject());
                    sc.chains.push_back(ch);
                    ++ci;
                }
            }
        }

        out.scenes.push_back(std::move(sc));
        ++si;
    }

    return true;
}

} // namespace forge7
