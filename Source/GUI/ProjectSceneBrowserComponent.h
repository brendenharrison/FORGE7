#pragma once

#include <functional>
#include <memory>
#include <set>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Storage/ProjectBrowserMetadata.h"

namespace forge7
{

struct AppContext;

/** Full-screen overlay: fast Project / Scene / Chain navigation (touch + encoder). */
class ProjectSceneBrowserComponent final : public juce::Component
{
public:
    explicit ProjectSceneBrowserComponent(AppContext& context,
                                          std::function<void()> onCloseRequestCallback);

    ~ProjectSceneBrowserComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Rescan library folder and rebuild rows (call when showing). */
    void rescanAndRebuild();

    void syncEncoderFocus();

private:
    class ListArea;

    struct HitRow
    {
        int logicalIndex { -1 };
    };

    struct LayoutRow
    {
        enum class Kind
        {
            Project,
            Scene,
            Chain
        };

        Kind kind { Kind::Project };
        juce::Rectangle<int> bounds;
        int projectVectorIndex { -1 };
        int sceneIndex { -1 };
        int chainIndex { -1 };
        juce::String primaryText;
        juce::String suffixCurrent;
        bool isExpandable { false };
        bool isExpanded { false };
    };

    void rebuildFlatRows();
    void layoutListGeometry();
    HitRow hitTestRows(juce::Point<int> p) const;
    void activateLogicalRow(int logicalIndex);
    void moveListFocus(int delta);
    void paintList(juce::Graphics& g, const juce::Rectangle<int>& listArea);
    void listMouseDown(const juce::MouseEvent& e);

    bool isSameProjectRow(const ProjectBrowserProjectInfo& info) const;
    void handleSelectProject(int projectVectorIndex, bool expandToggle);
    void handleSelectScene(int projectVectorIndex, int sceneIndex);
    void handleSelectChain(int projectVectorIndex, int sceneIndex, int chainIndex);

    AppContext& appContext;
    std::function<void()> closeCallback;

    juce::TextButton backButton { "Back" };
    juce::Label titleLabel;
    juce::Label contextProjectLabel;
    juce::Label contextSceneLabel;
    juce::Label contextChainLabel;
    juce::Label dirtyBannerLabel;
    juce::Label hintLabel;

    juce::Viewport listViewport;
    std::unique_ptr<ListArea> listArea;

    std::vector<ProjectBrowserProjectInfo> projectInfos;
    std::set<juce::String> expandedProjectKeys;

    std::vector<LayoutRow> layoutRows;
    int logicalRowCount { 0 };
    int focusedLogicalRow { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectSceneBrowserComponent)
};

} // namespace forge7
