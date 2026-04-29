#include "ProjectSceneBrowserComponent.h"

#include "../App/AppConfig.h"
#include "../App/AppContext.h"
#include "../App/ProjectSession.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"
#include "../Storage/ForgeStoragePaths.h"
#include "../Storage/ProjectBrowserMetadata.h"
#include "NavigationStatus.h"
#include "ProjectLibraryDialogs.h"

#include "../Controls/EncoderFocusTypes.h"
#include "../Controls/EncoderNavigator.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{

constexpr int kCardInsetX = 18;
constexpr int kCardInsetY = 14;
constexpr int kTopBarH = 56;
constexpr int kContextBlockH = 74;
constexpr int kDirtyBannerH = 24;
constexpr int kHintBarH = 36;

constexpr int kRowProjectH = 56;
constexpr int kRowSceneH = 52;
constexpr int kRowChainH = 46;

juce::String projectRowKey(const ProjectBrowserProjectInfo& p)
{
    if (p.isLiveSessionPlaceholder)
        return "__live__";

    return p.projectFile.getFullPathName();
}

juce::Colour bgDim() noexcept
{
    return juce::Colour(0xff0d0f12);
}

juce::Colour panel() noexcept
{
    return juce::Colour(0xff161a20);
}

juce::Colour textHi() noexcept
{
    return juce::Colour(0xffe8eaed);
}

juce::Colour textMuted() noexcept
{
    return juce::Colour(0xff8a9099);
}

juce::Colour accent() noexcept
{
    return juce::Colour(0xff6bc4ff);
}

void fillLiveProjectFromSession(AppContext& ctx, ProjectBrowserProjectInfo& out)
{
    out = {};
    out.isLiveSessionPlaceholder = true;
    out.isCurrentProject = true;

    if (ctx.getProjectDisplayName != nullptr)
        out.projectName = ctx.getProjectDisplayName();

    if (out.projectName.isEmpty())
        out.projectName = "Untitled Project";

    if (ctx.sceneManager == nullptr)
        return;

    int si = 0;

    for (const auto& scPtr : ctx.sceneManager->getScenes())
    {
        if (scPtr == nullptr)
        {
            ++si;
            continue;
        }

        ProjectBrowserSceneInfo sc;
        sc.sceneIndex = si;
        sc.sceneName = scPtr->getSceneName();

        int ci = 0;

        for (const auto& cvPtr : scPtr->getVariations())
        {
            if (cvPtr == nullptr)
            {
                ++ci;
                continue;
            }

            ProjectBrowserChainInfo ch;
            ch.chainIndex = ci;
            ch.chainName = cvPtr->getVariationName();
            sc.chains.push_back(ch);
            ++ci;
        }

        out.scenes.push_back(std::move(sc));
        ++si;
    }
}

} // namespace

class ProjectSceneBrowserComponent::ListArea final : public juce::Component
{
public:
    explicit ListArea(ProjectSceneBrowserComponent& o)
        : owner(o)
    {
    }

    void paint(juce::Graphics& g) override { owner.paintList(g, getLocalBounds()); }

    void mouseDown(const juce::MouseEvent& e) override { owner.listMouseDown(e); }

private:
    ProjectSceneBrowserComponent& owner;
};

ProjectSceneBrowserComponent::ProjectSceneBrowserComponent(AppContext& context,
                                                           std::function<void()> onCloseRequestCallback)
    : appContext(context)
    , closeCallback(std::move(onCloseRequestCallback))
{
    setOpaque(false);

    titleLabel.setText("Jump Browser", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, textHi());
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    backButton.onClick = [this]()
    {
        if (closeCallback != nullptr)
            closeCallback();
    };
    addAndMakeVisible(backButton);

    for (auto* l : {&contextProjectLabel, &contextSceneLabel, &contextChainLabel})
    {
        l->setFont(juce::Font(15.0f));
        l->setColour(juce::Label::textColourId, textMuted());
        l->setJustificationType(juce::Justification::centredLeft);
        l->setMinimumHorizontalScale(0.7f);
        addAndMakeVisible(*l);
    }

    dirtyBannerLabel.setFont(juce::Font(14.0f));
    dirtyBannerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb74d));
    dirtyBannerLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dirtyBannerLabel);

    hintLabel.setText("Rotate: Move   Press: Open/Select   Long Press: Close",
                      juce::dontSendNotification);
    hintLabel.setFont(juce::Font(13.0f));
    hintLabel.setColour(juce::Label::textColourId, textMuted());
    hintLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(hintLabel);

    listArea = std::make_unique<ListArea>(*this);
    listViewport.setViewedComponent(listArea.get(), false);
    listViewport.setScrollBarsShown(true, false);
    listViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(listViewport);
}

ProjectSceneBrowserComponent::~ProjectSceneBrowserComponent() = default;

void ProjectSceneBrowserComponent::rescanAndRebuild()
{
    projectInfos.clear();
    expandedProjectKeys.clear();

    ensureForgeStorageFoldersExist();

    juce::String lastPath;

    if (appContext.appConfig != nullptr)
        lastPath = appContext.appConfig->getLastLoadedProjectPath();

    bool matchedDisk = false;

    {
        const auto files = listLibraryProjectFiles();

        for (const auto& f : files)
        {
            ProjectBrowserProjectInfo info;

            if (! loadProjectBrowserMetadata(f, info))
                continue;

            if (lastPath.isNotEmpty() && f.getFullPathName() == lastPath)
            {
                info.isCurrentProject = true;
                matchedDisk = true;
            }

            projectInfos.push_back(std::move(info));
        }
    }

    if (! matchedDisk && appContext.sceneManager != nullptr)
    {
        ProjectBrowserProjectInfo live;
        fillLiveProjectFromSession(appContext, live);
        projectInfos.insert(projectInfos.begin(), std::move(live));
    }

    for (size_t i = 0; i < projectInfos.size(); ++i)
    {
        if (projectInfos[i].isCurrentProject)
            expandedProjectKeys.insert(projectRowKey(projectInfos[i]));
    }

    rebuildFlatRows();
    layoutListGeometry();
    repaint();
}

void ProjectSceneBrowserComponent::rebuildFlatRows()
{
    layoutRows.clear();
    logicalRowCount = 0;

    auto appendRow = [&](LayoutRow r)
    {
        layoutRows.push_back(std::move(r));
        ++logicalRowCount;
    };

    if (projectInfos.empty())
    {
        LayoutRow r;
        r.kind = LayoutRow::Kind::Project;
        r.primaryText = "No projects saved yet";
        r.projectVectorIndex = -1;
        appendRow(std::move(r));
        focusedLogicalRow = 0;
        return;
    }

    for (int pi = 0; pi < static_cast<int>(projectInfos.size()); ++pi)
    {
        const auto& proj = projectInfos[static_cast<size_t>(pi)];
        const juce::String key = projectRowKey(proj);
        const bool expanded = expandedProjectKeys.count(key) > 0;

        LayoutRow pr;
        pr.kind = LayoutRow::Kind::Project;
        pr.projectVectorIndex = pi;
        pr.isExpandable = true;
        pr.isExpanded = expanded;
        pr.primaryText = proj.projectName;
        pr.suffixCurrent = proj.isCurrentProject ? juce::String("Current") : juce::String();

        if (proj.isLiveSessionPlaceholder)
            pr.primaryText += "   (session)";

        appendRow(std::move(pr));

        if (! expanded)
            continue;

        for (const auto& sc : proj.scenes)
        {
            LayoutRow sr;
            sr.kind = LayoutRow::Kind::Scene;
            sr.projectVectorIndex = pi;
            sr.sceneIndex = sc.sceneIndex;
            sr.primaryText = "Scene " + juce::String(sc.sceneIndex + 1) + ": " + sc.sceneName;

            const bool curSc =
                proj.isCurrentProject && appContext.sceneManager != nullptr
                && appContext.sceneManager->getActiveSceneIndex() == sc.sceneIndex;

            sr.suffixCurrent = curSc ? juce::String("Current") : juce::String();

            appendRow(std::move(sr));

            for (const auto& ch : sc.chains)
            {
                LayoutRow cr;
                cr.kind = LayoutRow::Kind::Chain;
                cr.projectVectorIndex = pi;
                cr.sceneIndex = sc.sceneIndex;
                cr.chainIndex = ch.chainIndex;
                juce::String name = ch.chainName;

                if (name.isEmpty())
                    name = "Chain";

                cr.primaryText =
                    "Chain " + juce::String(ch.chainIndex + 1) + " - " + name;

                const bool curCh =
                    curSc && appContext.sceneManager != nullptr
                    && appContext.sceneManager->getActiveChainVariationIndex() == ch.chainIndex;

                cr.suffixCurrent = curCh ? juce::String("Current") : juce::String();

                appendRow(std::move(cr));
            }
        }
    }

    focusedLogicalRow = juce::jlimit(0, juce::jmax(0, logicalRowCount - 1), focusedLogicalRow);
}

void ProjectSceneBrowserComponent::layoutListGeometry()
{
    const int scrollbarAllowance = 18;

    const int viewportW = listViewport.getLocalBounds().getWidth();

    const int fallbackW =
        juce::jmax(320, getWidth() - (kCardInsetX * 2) - scrollbarAllowance - 40);

    const int contentW = viewportW > 0 ? juce::jmax(320, viewportW - scrollbarAllowance) : fallbackW;

    int y = 0;

    for (auto& row : layoutRows)
    {
        int h = kRowProjectH;

        if (row.kind == LayoutRow::Kind::Scene)
            h = kRowSceneH;
        else if (row.kind == LayoutRow::Kind::Chain)
            h = kRowChainH;

        row.bounds = juce::Rectangle<int>(0, y, contentW, h);
        y += h;
    }

    const int contentH = juce::jmax(y, listViewport.getHeight());

    if (listArea != nullptr)
        listArea->setSize(contentW, contentH);

    repaint();
}

ProjectSceneBrowserComponent::HitRow ProjectSceneBrowserComponent::hitTestRows(juce::Point<int> p) const
{
    HitRow hr;

    for (int i = 0; i < static_cast<int>(layoutRows.size()); ++i)
    {
        if (layoutRows[static_cast<size_t>(i)].bounds.contains(p))
        {
            hr.logicalIndex = i;
            break;
        }
    }

    return hr;
}

void ProjectSceneBrowserComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.78f));

    const auto card = getLocalBounds().reduced(kCardInsetX, kCardInsetY).toFloat();
    const float corner = 12.0f;

    g.setColour(juce::Colour(0xff12161e));
    g.fillRoundedRectangle(card, corner);

    g.setColour(juce::Colour(0xff2a3544));
    g.drawRoundedRectangle(card.reduced(0.5f, 0.5f), corner, 1.0f);
}

void ProjectSceneBrowserComponent::paintList(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    g.fillAll(bgDim());

    const int padL = 10;

    for (int i = 0; i < static_cast<int>(layoutRows.size()); ++i)
    {
        const auto& row = layoutRows[static_cast<size_t>(i)];
        const auto r = row.bounds;

        juce::Colour rowBg = panel();

        if (i == focusedLogicalRow)
            rowBg = panel().brighter(0.12f);

        g.setColour(rowBg);
        g.fillRect(r);

        if (i == focusedLogicalRow)
        {
            g.setColour(accent().withAlpha(0.85f));
            g.drawRect(r, 2);
        }

        int indent = padL;

        if (row.kind == LayoutRow::Kind::Scene)
            indent += 18;
        else if (row.kind == LayoutRow::Kind::Chain)
            indent += 36;

        auto textR = r.reduced(indent, 4);

        if (row.kind == LayoutRow::Kind::Project && row.isExpandable)
        {
            juce::String arrow = row.isExpanded ? "-" : "+";
            g.setColour(textHi());
            g.setFont(juce::Font(20.0f, juce::Font::bold));
            g.drawText(arrow, textR.removeFromLeft(28), juce::Justification::centred, false);
        }

        g.setColour(textHi());
        g.setFont(row.kind == LayoutRow::Kind::Project ? juce::Font(17.5f) : juce::Font(16.0f));

        auto leftText = textR;

        if (row.suffixCurrent.isNotEmpty())
        {
            auto right = leftText.removeFromRight(120);
            g.setColour(accent());
            g.setFont(juce::Font(14.0f, juce::Font::italic));
            g.drawFittedText(row.suffixCurrent, right, juce::Justification::centredRight, 1);
            g.setColour(textHi());
        }

        g.drawFittedText(row.primaryText, leftText, juce::Justification::centredLeft, 2);
    }
}

void ProjectSceneBrowserComponent::listMouseDown(const juce::MouseEvent& e)
{
    const auto local = e.getPosition();
    const HitRow hr = hitTestRows(local);

    if (hr.logicalIndex < 0)
        return;

    focusedLogicalRow = hr.logicalIndex;
    activateLogicalRow(hr.logicalIndex);
    repaint();

    if (listArea != nullptr)
        listArea->repaint();
}

void ProjectSceneBrowserComponent::moveListFocus(const int delta)
{
    if (logicalRowCount <= 0 || delta == 0)
        return;

    focusedLogicalRow += delta;

    while (focusedLogicalRow < 0)
        focusedLogicalRow += logicalRowCount;

    focusedLogicalRow %= logicalRowCount;

    Logger::info("FORGE7 JumpBrowser: focused row=" + juce::String(focusedLogicalRow));

    if (listArea != nullptr
        && juce::isPositiveAndBelow(focusedLogicalRow, static_cast<int>(layoutRows.size())))
    {
        const auto& r = layoutRows[static_cast<size_t>(focusedLogicalRow)].bounds;
        const int vpH = listViewport.getHeight();
        const int rowCentreY = r.getCentreY();
        int newY = rowCentreY - vpH / 2;
        const int maxScroll = juce::jmax(0, listArea->getHeight() - vpH);
        newY = juce::jlimit(0, maxScroll, newY);
        listViewport.setViewPosition(listViewport.getViewPositionX(), newY);
    }

    repaint();

    if (listArea != nullptr)
        listArea->repaint();
}

bool ProjectSceneBrowserComponent::isSameProjectRow(const ProjectBrowserProjectInfo& info) const
{
    return info.isCurrentProject;
}

void ProjectSceneBrowserComponent::handleSelectProject(const int projectVectorIndex,
                                                       const bool /*expandToggle*/)
{
    if (! juce::isPositiveAndBelow(projectVectorIndex, static_cast<int>(projectInfos.size())))
        return;

    const auto& info = projectInfos[static_cast<size_t>(projectVectorIndex)];
    const juce::String key = projectRowKey(info);

    if (expandedProjectKeys.count(key) > 0)
        expandedProjectKeys.erase(key);
    else
        expandedProjectKeys.insert(key);

    rebuildFlatRows();
    layoutListGeometry();
    syncEncoderFocus();
    repaint();
}

void ProjectSceneBrowserComponent::handleSelectScene(const int projectVectorIndex, const int sceneIndex)
{
    if (! juce::isPositiveAndBelow(projectVectorIndex, static_cast<int>(projectInfos.size())))
        return;

    const auto& info = projectInfos[static_cast<size_t>(projectVectorIndex)];

    if (! info.isLiveSessionPlaceholder && ! isSameProjectRow(info))
    {
        openLibraryProjectFileReplacingCurrent(this, appContext, info.projectFile, sceneIndex, -1);

        if (closeCallback != nullptr)
            closeCallback();

        return;
    }

    if (appContext.projectSession == nullptr)
        return;

    appContext.projectSession->switchToScene(sceneIndex);

    if (closeCallback != nullptr)
        closeCallback();
}

void ProjectSceneBrowserComponent::handleSelectChain(const int projectVectorIndex,
                                                     const int sceneIndex,
                                                     const int chainIndex)
{
    if (! juce::isPositiveAndBelow(projectVectorIndex, static_cast<int>(projectInfos.size())))
        return;

    const auto& info = projectInfos[static_cast<size_t>(projectVectorIndex)];

    if (! info.isLiveSessionPlaceholder && ! isSameProjectRow(info))
    {
        openLibraryProjectFileReplacingCurrent(this, appContext, info.projectFile, sceneIndex, chainIndex);

        if (closeCallback != nullptr)
            closeCallback();

        return;
    }

    if (appContext.projectSession == nullptr)
        return;

    if (appContext.sceneManager != nullptr
        && appContext.sceneManager->getActiveSceneIndex() != sceneIndex)
        appContext.projectSession->switchToScene(sceneIndex);

    appContext.projectSession->switchToChain(chainIndex);

    if (closeCallback != nullptr)
        closeCallback();
}

void ProjectSceneBrowserComponent::activateLogicalRow(const int logicalIndex)
{
    if (! juce::isPositiveAndBelow(logicalIndex, static_cast<int>(layoutRows.size())))
        return;

    const auto& row = layoutRows[static_cast<size_t>(logicalIndex)];

    if (row.primaryText == "No projects saved yet")
        return;

    switch (row.kind)
    {
        case LayoutRow::Kind::Project:
            handleSelectProject(row.projectVectorIndex, true);
            break;

        case LayoutRow::Kind::Scene:
            handleSelectScene(row.projectVectorIndex, row.sceneIndex);
            break;

        case LayoutRow::Kind::Chain:
            handleSelectChain(row.projectVectorIndex, row.sceneIndex, row.chainIndex);
            break;

        default:
            break;
    }
}

void ProjectSceneBrowserComponent::resized()
{
    auto area = getLocalBounds().reduced(kCardInsetX, kCardInsetY);

    auto top = area.removeFromTop(kTopBarH).reduced(10, 8);
    backButton.setBounds(top.removeFromLeft(100).reduced(0, 6));
    top.removeFromLeft(10);
    titleLabel.setBounds(top);

    auto ctx = area.removeFromTop(kContextBlockH).reduced(12, 4);
    contextProjectLabel.setBounds(ctx.removeFromTop(22));
    contextSceneLabel.setBounds(ctx.removeFromTop(22));
    contextChainLabel.setBounds(ctx.removeFromTop(22));

    if (appContext.projectSession != nullptr && appContext.projectSession->isProjectDirty())
    {
        dirtyBannerLabel.setText("Current project has unsaved changes", juce::dontSendNotification);
        dirtyBannerLabel.setVisible(true);
        auto d = area.removeFromTop(kDirtyBannerH).reduced(12, 4);
        dirtyBannerLabel.setBounds(d);
    }
    else
    {
        dirtyBannerLabel.setVisible(false);
    }

    hintLabel.setBounds(area.removeFromBottom(kHintBarH).reduced(12, 6));

    listViewport.setBounds(area.reduced(10, 4));
    layoutListGeometry();

    const NavigationStatus nav = computeNavigationStatus(appContext);

    contextProjectLabel.setText(nav.getProjectHeaderLine(), juce::dontSendNotification);

    juce::String scLine = "Scene: ";

    if (nav.hasActiveScene())
        scLine += (nav.sceneName.isNotEmpty() ? nav.sceneName : juce::String("Untitled"));
    else
        scLine += "-";

    contextSceneLabel.setText(scLine, juce::dontSendNotification);

    contextChainLabel.setText("Chain: " + nav.getChainDisplayLabel(), juce::dontSendNotification);

    syncEncoderFocus();
}

void ProjectSceneBrowserComponent::onBrowserShown()
{
    layoutListGeometry();
    syncEncoderFocus();
    repaint();

    Logger::info("FORGE7 JumpBrowser: viewport bounds=" + listViewport.getBounds().toString()
                   + " listArea size="
                   + (listArea != nullptr
                          ? juce::String(listArea->getWidth()) + " x " + juce::String(listArea->getHeight())
                          : juce::String("(null)"))
                   + " rows=" + juce::String(logicalRowCount));
    Logger::info("FORGE7 JumpBrowser: open bounds=" + getBounds().toString());
    Logger::info("FORGE7 JumpBrowser: modal encoder focus set rows=" + juce::String(logicalRowCount));
}

void ProjectSceneBrowserComponent::syncEncoderFocus()
{
    if (appContext.encoderNavigator == nullptr)
        return;

    std::vector<EncoderFocusItem> items;

    juce::Component::SafePointer<juce::Component> safeVp(&listViewport);

    /** List viewport first so primary navigation starts on rows; Back secondary. */
    items.push_back(
        {&listViewport,
         [this, safeVp]()
         {
             if (safeVp != nullptr)
                 activateLogicalRow(focusedLogicalRow);
         },
         [this](int d)
         {
             moveListFocus(d > 0 ? 1 : (d < 0 ? -1 : 0));
         }});

    items.push_back({&backButton,
                     [this]()
                     {
                         if (backButton.isEnabled())
                             backButton.triggerClick();
                     },
                     {}});

    appContext.encoderNavigator->setModalFocusChain(std::move(items));
}

} // namespace forge7
