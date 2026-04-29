#include "RackViewComponent.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

#include "../Scene/Scene.h"

#include "../App/AppContext.h"
#include "../Controls/EncoderFocusTypes.h"
#include "../App/MainComponent.h"
#include "../Audio/AudioEngine.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"
#include "../App/ProjectSession.h"
#include "../Scene/ChainVariation.h"
#include "../Scene/SceneManager.h"
#include "../Utilities/Logger.h"
#include "CpuMeter.h"
#include "NameEntryModal.h"
#include "NavigationStatus.h"
#include "PluginBrowserComponent.h"
#include "ChainControlsPanelComponent.h"
#include "RackSlotCard.h"

namespace forge7
{
namespace
{

juce::Colour rackBackground() noexcept { return juce::Colour(0xff12151a); }
juce::Colour rackSurface() noexcept { return juce::Colour(0xff1a1d23); }
juce::Colour rackAccent() noexcept { return juce::Colour(0xff4a9eff); }
juce::Colour rackText() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour rackMuted() noexcept { return juce::Colour(0xff8a9099); }

void styleLargeTextButton(juce::Button& b)
{
    b.setColour(juce::TextButton::buttonColourId, rackSurface().brighter(0.08f));
    b.setColour(juce::TextButton::textColourOffId, rackText());
}

void styleBottomNavButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    b.setColour(juce::TextButton::textColourOffId, rackText());
}

/** Full-screen underlay for Plugin Browser (in-app surface, not a floating OS window). */
class RackBrowserBackdrop final : public juce::Component
{
public:
    void paint(juce::Graphics& g) override { g.fillAll(rackBackground()); }
};
} // namespace

void RackViewComponent::AddPluginCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(rackSurface().brighter(0.05f));
    g.fillRoundedRectangle(bounds, 12.0f);

    g.setColour(rackAccent().withAlpha(0.45f));
    g.drawRoundedRectangle(bounds, 12.0f, 2.0f);

    g.setColour(rackAccent());
    g.setFont(juce::Font(34.0f, juce::Font::bold));
    g.drawText("+", getLocalBounds().removeFromTop(getHeight() / 2), juce::Justification::centred, false);

    g.setColour(rackText());
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("Add Plugin", getLocalBounds().removeFromBottom(28), juce::Justification::centred, false);
}

void RackViewComponent::AddPluginCard::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    if (onAddClicked != nullptr)
        onAddClicked();
}

RackViewComponent::IoBlock::IoBlock(const Kind k)
    : kind(k)
{
    setInterceptsMouseClicks(false, false);
}

void RackViewComponent::IoBlock::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(rackSurface().brighter(0.06f));
    g.fillRoundedRectangle(bounds, 10.0f);

    g.setColour(rackAccent().withAlpha(0.35f));
    g.drawRoundedRectangle(bounds, 10.0f, 2.0f);

    g.setColour(rackText());
    g.setFont(juce::Font(18.0f));

    const auto label = (kind == Kind::Input) ? "INPUT" : "OUTPUT";
    g.drawText(label, getLocalBounds(), juce::Justification::centred);
}

RackViewComponent::RackViewComponent(AppContext& context)
    : appContext(context)
{
    jassert(appContext.pluginHostManager != nullptr);
    jassert(appContext.sceneManager != nullptr);

    setOpaque(true);

    projectHeaderLabel.setJustificationType(juce::Justification::centredLeft);
    projectHeaderLabel.setFont(juce::Font(12.0f));
    projectHeaderLabel.setColour(juce::Label::textColourId, rackMuted());
    projectHeaderLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(projectHeaderLabel);

    projectDirtyLabel.setJustificationType(juce::Justification::centredLeft);
    projectDirtyLabel.setFont(juce::Font(11.0f));
    projectDirtyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb74d));
    projectDirtyLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(projectDirtyLabel);

    sceneTitleLabel.setJustificationType(juce::Justification::centredLeft);
    sceneTitleLabel.setFont(juce::Font(17.0f));
    sceneTitleLabel.setColour(juce::Label::textColourId, rackText());
    addAndMakeVisible(sceneTitleLabel);

    chainHeaderLabel.setJustificationType(juce::Justification::centredLeft);
    chainHeaderLabel.setFont(juce::Font(15.0f));
    chainHeaderLabel.setColour(juce::Label::textColourId, rackMuted());
    addAndMakeVisible(chainHeaderLabel);

    chainCountLabel.setJustificationType(juce::Justification::centred);
    chainCountLabel.setFont(juce::Font(13.0f));
    chainCountLabel.setColour(juce::Label::textColourId, rackMuted());
    addAndMakeVisible(chainCountLabel);

    auto wireChainNav = [this](juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, rackSurface().brighter(0.08f));
        b.setColour(juce::TextButton::textColourOffId, rackText());
        b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible(b);
    };

    wireChainNav(chainPrevButton);
    wireChainNav(chainNextButton);
    wireChainNav(addChainButton);
    wireChainNav(renameChainButton);
    wireChainNav(addSceneButton);
    wireChainNav(renameSceneButton);

    chainPrevButton.onClick = [this]()
    {
        if (appContext.projectSession == nullptr)
            return;

        const int before = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveChainVariationIndex()
                                                              : -1;
        appContext.projectSession->previousChain();
        const int after = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveChainVariationIndex()
                                                             : -1;

        if (before == 0 && after != 0)
            Logger::info("FORGE7: Chain - wrapped from first to last");

        refreshSlotDisplays();
    };

    chainNextButton.onClick = [this]()
    {
        if (appContext.projectSession == nullptr)
            return;

        const int before = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveChainVariationIndex()
                                                                : -1;
        appContext.projectSession->nextChain();
        const int after = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveChainVariationIndex()
                                                             : -1;

        if (after == 0 && before != 0)
            Logger::info("FORGE7: Chain + wrapped from last to first");

        refreshSlotDisplays();
    };

    addChainButton.onClick = [this]() { promptAddChain(); };
    renameChainButton.onClick = [this]() { promptRenameActiveChain(); };
    addSceneButton.onClick = [this]() { promptAddScene(); };
    renameSceneButton.onClick = [this]() { promptRenameActiveScene(); };

    editModeBadge.setText("EDIT MODE", juce::dontSendNotification);
    editModeBadge.setJustificationType(juce::Justification::centred);
    editModeBadge.setFont(juce::Font(14.0f));
    editModeBadge.setColour(juce::Label::textColourId, rackAccent());
    editModeBadge.setColour(juce::Label::backgroundColourId, rackAccent().withAlpha(0.15f));
    editModeBadge.setBorderSize(juce::BorderSize<int>(6, 12, 6, 12));
    addAndMakeVisible(editModeBadge);

    tempoLabel.setJustificationType(juce::Justification::centredRight);
    tempoLabel.setFont(juce::Font(16.0f));
    tempoLabel.setColour(juce::Label::textColourId, rackText());
    addAndMakeVisible(tempoLabel);

    if (appContext.audioEngine != nullptr)
        cpuMeter = std::make_unique<CpuMeter>(appContext.audioEngine);

    if (cpuMeter != nullptr)
        addAndMakeVisible(*cpuMeter);

    globalBypassFxToggle.setClickingTogglesState(true);

    if (appContext.audioEngine != nullptr)
        globalBypassFxToggle.setToggleState(appContext.audioEngine->isGlobalBypass(), juce::dontSendNotification);

    globalBypassFxToggle.onClick = [this]()
    {
        if (appContext.audioEngine != nullptr)
            appContext.audioEngine->setGlobalBypass(globalBypassFxToggle.getToggleState());

        if (appContext.projectSession != nullptr)
            appContext.projectSession->markProjectDirty();
    };

    globalBypassFxToggle.setColour(juce::ToggleButton::textColourId, rackText());
    globalBypassFxToggle.setColour(juce::ToggleButton::tickColourId, rackAccent());
    addAndMakeVisible(globalBypassFxToggle);

    inputBlock = std::make_unique<IoBlock>(IoBlock::Kind::Input);
    outputBlock = std::make_unique<IoBlock>(IoBlock::Kind::Output);
    addAndMakeVisible(*inputBlock);
    addAndMakeVisible(*outputBlock);

    addPluginCard = std::make_unique<AddPluginCard>();
    addPluginCard->onAddClicked = [this]()
    {
        if (appContext.pluginHostManager == nullptr)
            return;

        auto* chain = appContext.pluginHostManager->getPluginChain();
        if (chain == nullptr)
            return;

        const int nextEmpty = findFirstEmptyBackendSlotIndex();
        if (nextEmpty < 0)
            return;

        selectionBeforePendingAdd = selectedSlotIndex;
        pendingAddSlotIndex = nextEmpty;
        browserOpenReason = BrowserOpenReason::AddNewSlot;

        Logger::info("FORGE7 RackAdd: add card clicked targetSlot=" + juce::String(nextEmpty)
                     + " selectedBefore=" + juce::String(selectionBeforePendingAdd));

        setSelectedSlot(pendingAddSlotIndex);
        showPluginBrowser();
    };

    for (auto& a : arrowLabels)
    {
        a = std::make_unique<juce::Label>();
        a->setText(">", juce::dontSendNotification);
        a->setJustificationType(juce::Justification::centred);
        a->setColour(juce::Label::textColourId, rackMuted());
        a->setFont(juce::Font(28.0f));
        a->setInterceptsMouseClicks(false, false);
        // Arrows belong to the center lane content.
    }

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        slotCards[static_cast<size_t>(i)] = std::make_unique<RackSlotCard>();
        slotCards[static_cast<size_t>(i)]->setSlotIndex(i);
        slotCards[static_cast<size_t>(i)]->setShowInlineControls(false);
        wireSlotCallbacks(i);
        addChildComponent(*slotCards[static_cast<size_t>(i)]);
    }

    chainContent = std::make_unique<juce::Component>();
    chainContent->setInterceptsMouseClicks(true, true);

    for (auto& a : arrowLabels)
        chainContent->addAndMakeVisible(*a);
    for (auto& c : slotCards)
        if (c != nullptr)
            chainContent->addChildComponent(*c);
    if (addPluginCard != nullptr)
        chainContent->addAndMakeVisible(*addPluginCard);

    chainViewport.setViewedComponent(chainContent.get(), false);
    chainViewport.setScrollBarsShown(false, true); // no vertical scroll; center lane may scroll horizontally later
    chainViewport.getHorizontalScrollBar().setColour(juce::ScrollBar::thumbColourId, rackAccent().withAlpha(0.55f));
    addAndMakeVisible(chainViewport);

    navPerformanceButton.setButtonText("Performance");

    {
        styleBottomNavButton(navPerformanceButton);
        navPerformanceButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible(navPerformanceButton);
    }

    navPerformanceButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->setEditMode(false);
    };

    auto wireCtxText = [this](juce::TextButton& b)
    {
        styleLargeTextButton(b);
        b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible(b);
    };

    ctxBypassToggle.setClickingTogglesState(true);
    ctxBypassToggle.setColour(juce::ToggleButton::textColourId, rackText());
    ctxBypassToggle.setColour(juce::ToggleButton::tickColourId, rackAccent());
    ctxBypassToggle.onClick = [this]()
    {
        if (appContext.pluginHostManager == nullptr || selectedSlotIndex < 0)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
            c->bypassSlot(selectedSlotIndex, ctxBypassToggle.getToggleState());

        if (appContext.projectSession != nullptr)
            appContext.projectSession->markProjectDirty();
    };
    addAndMakeVisible(ctxBypassToggle);

    wireCtxText(ctxMoveLeftButton);
    wireCtxText(ctxMoveRightButton);
    wireCtxText(ctxRemoveButton);
    wireCtxText(ctxReplaceButton);
    wireCtxText(ctxEditorButton);

    ctxRemoveButton.onClick = [this]()
    {
        if (appContext.pluginHostManager == nullptr || selectedSlotIndex < 0)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
            c->removePluginFromSlot(selectedSlotIndex);

        if (appContext.projectSession != nullptr)
            appContext.projectSession->markProjectDirty();

        refreshSlotDisplays();
    };

    ctxReplaceButton.onClick = [this]()
    {
        if (selectedSlotIndex < 0)
            return;

        pendingAddSlotIndex = -1;
        browserOpenReason = BrowserOpenReason::ReplaceExistingSlot;

        Logger::info("FORGE7 RackAdd: Replace clicked replaceSlot=" + juce::String(selectedSlotIndex));

        showPluginBrowser();
    };

    ctxEditorButton.onClick = [this]()
    {
        if (selectedSlotIndex < 0)
            return;

        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->openFullscreenPluginEditor(selectedSlotIndex);
    };

    ctxMoveLeftButton.onClick = [this]()
    {
        if (appContext.pluginHostManager == nullptr || selectedSlotIndex <= 0)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
        {
            const int from = selectedSlotIndex;

            if (c->moveSlot(from, from - 1))
                setSelectedSlot(from - 1);
        }

        if (appContext.projectSession != nullptr)
            appContext.projectSession->markProjectDirty();

        refreshSlotDisplays();
    };

    ctxMoveRightButton.onClick = [this]()
    {
        if (appContext.pluginHostManager == nullptr || selectedSlotIndex < 0
            || selectedSlotIndex >= kPluginChainMaxSlots - 1)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
        {
            const int from = selectedSlotIndex;

            if (c->moveSlot(from, from + 1))
                setSelectedSlot(from + 1);
        }

        if (appContext.projectSession != nullptr)
            appContext.projectSession->markProjectDirty();

        refreshSlotDisplays();
    };

    settingsButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->openSettings();
    };

    styleLargeTextButton(settingsButton);
    addAndMakeVisible(settingsButton);

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    styleLargeTextButton(simHwButton);
    simHwButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    simHwButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->toggleSimulatedControlsPanel();
    };
    addAndMakeVisible(simHwButton);
#endif

    browserOverlay = std::make_unique<RackBrowserBackdrop>();
    browserOverlay->setAlwaysOnTop(true);
    browserOverlay->setVisible(false);
    browserOverlay->setInterceptsMouseClicks(true, true);
    addChildComponent(*browserOverlay);

    pluginBrowser = std::make_unique<PluginBrowserComponent>(*appContext.pluginHostManager);

    pluginBrowser->onPluginChosen = [this](const juce::PluginDescription& d)
    {
        if (loadPluginIntoSelectedSlot(d))
            dismissPluginBrowserOverlay(false);
    };

    pluginBrowser->onDismiss = [this]()
    {
        dismissPluginBrowserOverlay(true);
    };

    browserOverlay->addAndMakeVisible(*pluginBrowser);

    pluginBrowser->setInterceptsMouseClicks(true, true);
    pluginBrowser->toFront(false);

    chainControlsPanel = std::make_unique<ChainControlsPanelComponent>(appContext);
    addAndMakeVisible(*chainControlsPanel);

    startTimerHz(4);
}

RackViewComponent::~RackViewComponent()
{
    stopTimer();
}

void RackViewComponent::paint(juce::Graphics& g)
{
    g.fillAll(rackBackground());
}

void RackViewComponent::resized()
{
    auto area = getLocalBounds();

    const int projectStripH = 34;
    const int statusH = 52;
    const int chainNavH = 44;
    const int contextH = 50;
    const int bottomH = 52;
    const int pad = 8;

    // Project header strip (small).
    auto projectStrip = area.removeFromTop(projectStripH).reduced(pad, 0);
    projectHeaderLabel.setBounds(projectStrip.removeFromTop(18));
    projectDirtyLabel.setBounds(projectStrip);

    // Top: scene title / chain / edit badge / tempo / global bypass / CPU
    auto statusArea = area.removeFromTop(statusH).reduced(pad, 4);

    if (cpuMeter != nullptr)
    {
        cpuMeter->setBounds(statusArea.removeFromRight(96).reduced(0, 2));
        statusArea.removeFromRight(6);
    }

    globalBypassFxToggle.setBounds(statusArea.removeFromRight(100).reduced(0, 2));
    statusArea.removeFromRight(6);

    tempoLabel.setBounds(statusArea.removeFromRight(100).reduced(0, 2));
    statusArea.removeFromRight(8);

    editModeBadge.setBounds(statusArea.removeFromRight(110).reduced(0, 4));
    statusArea.removeFromRight(8);

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    simHwButton.setBounds(statusArea.removeFromRight(92).reduced(0, 4));
    statusArea.removeFromRight(8);
#endif

    const int chainHeaderW = juce::jmin(260, juce::jmax(140, statusArea.getWidth() / 3));
    chainHeaderLabel.setBounds(statusArea.removeFromRight(chainHeaderW));
    statusArea.removeFromRight(6);
    sceneTitleLabel.setBounds(statusArea);

    area.removeFromTop(4);

    // Chain nav row: Chain - / count / Chain + / Add Chain / Rename Chain / Add Scene / Rename Scene
    auto chainNavRow = area.removeFromTop(chainNavH).reduced(pad, 4);

    const int navBtnW = 92;
    const int gap = 6;

    chainPrevButton.setBounds(chainNavRow.removeFromLeft(navBtnW).reduced(0, 2));
    chainNavRow.removeFromLeft(gap);

    chainCountLabel.setBounds(chainNavRow.removeFromLeft(110));
    chainNavRow.removeFromLeft(gap);

    chainNextButton.setBounds(chainNavRow.removeFromLeft(navBtnW).reduced(0, 2));
    chainNavRow.removeFromLeft(gap * 2);

    addChainButton.setBounds(chainNavRow.removeFromLeft(108).reduced(0, 2));
    chainNavRow.removeFromLeft(gap);
    renameChainButton.setBounds(chainNavRow.removeFromLeft(124).reduced(0, 2));
    chainNavRow.removeFromLeft(gap * 2);
    addSceneButton.setBounds(chainNavRow.removeFromLeft(108).reduced(0, 2));
    chainNavRow.removeFromLeft(gap);
    renameSceneButton.setBounds(chainNavRow.removeFromLeft(124).reduced(0, 2));

    area.removeFromTop(4);

    // Bottom: primary nav (Performance) + settings
    auto bottomArea = area.removeFromBottom(bottomH).reduced(pad, 4);

    settingsButton.setBounds(bottomArea.removeFromRight(100).reduced(0, 2));
    bottomArea.removeFromRight(8);

    navPerformanceButton.setBounds(bottomArea.reduced(4, 0));

    area.removeFromBottom(4);

    // Slot actions (hardware-style strip)
    auto ctxRow = area.removeFromBottom(contextH).reduced(pad, 2);
    const int ctxGap = 6;

    ctxEditorButton.setBounds(ctxRow.removeFromRight(92).reduced(0, 2));
    ctxRow.removeFromRight(ctxGap);
    ctxReplaceButton.setBounds(ctxRow.removeFromRight(100).reduced(0, 2));
    ctxRow.removeFromRight(ctxGap);
    ctxRemoveButton.setBounds(ctxRow.removeFromRight(88).reduced(0, 2));
    ctxRow.removeFromRight(ctxGap);
    ctxBypassToggle.setBounds(ctxRow.removeFromRight(100).reduced(0, 2));
    ctxRow.removeFromRight(ctxGap);
    ctxMoveRightButton.setBounds(ctxRow.removeFromRight(56).reduced(0, 2));
    ctxRow.removeFromRight(ctxGap);
    ctxMoveLeftButton.setBounds(ctxRow.removeFromRight(56).reduced(0, 2));

    area.removeFromBottom(4);

    constexpr int controlsBarH = 92;

    if (chainControlsPanel != nullptr)
    {
        chainControlsPanel->setVisible(true);
        chainControlsPanel->setBounds(area.removeFromBottom(controlsBarH).reduced(pad, 0));
    }

    area.removeFromBottom(4);

    // Chain region: fixed INPUT (left), fixed OUTPUT (right), center lane for visible plugins + Add card.
    const int ioW = 100;
    const int padChain = pad;
    auto chainArea = area.reduced(padChain, 0);

    auto leftIo = chainArea.removeFromLeft(ioW);
    chainArea.removeFromLeft(10);
    auto rightIo = chainArea.removeFromRight(ioW);
    chainArea.removeFromRight(10);

    if (inputBlock != nullptr)
        inputBlock->setBounds(leftIo.reduced(0, 8));
    if (outputBlock != nullptr)
        outputBlock->setBounds(rightIo.reduced(0, 8));

    chainViewport.setBounds(chainArea);

    layoutRackChain();

    syncEncoderFocus();
}

void RackViewComponent::layoutRackChain()
{
    if (chainContent == nullptr)
        return;

    const int arrowW = 30;
    const int gap = 10;
    const int pad = 8;
    const int stripH = juce::jmax(150, chainViewport.getHeight() > 0 ? chainViewport.getHeight() - 8 : 150);
    const int pluginW = 170;
    const int addW = 160;

    auto* chain = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;
    if (chain == nullptr)
        return;

    // Determine visible used slots (contiguous from slot 0).
    int lastUsed = -1;
    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        const auto info = chain->getSlotInfo(i);

        if (chainSlotShowsInRailLine(pendingAddSlotIndex, i, info))
            lastUsed = i;
        else
            break;
    }

    // Show only used cards (0..lastUsed), plus pending add if it is beyond lastUsed.
    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        const bool shouldShow = (i >= 0 && i <= lastUsed) || (i == pendingAddSlotIndex && i > lastUsed);
        if (slotCards[(size_t)i] != nullptr)
            slotCards[(size_t)i]->setVisible(shouldShow);
    }

    const bool chainFull = (lastUsed >= kPluginChainMaxSlots - 1) || (pendingAddSlotIndex == kPluginChainMaxSlots - 1 && lastUsed >= 0);

    const bool showAdd = !chainFull;
    if (addPluginCard != nullptr)
        addPluginCard->setVisible(showAdd);

    // Hide all arrows; we'll show only what we use.
    for (auto& a : arrowLabels)
        if (a != nullptr)
            a->setVisible(false);

    int x = pad;
    const int y = pad;

    // arrow after input (index 0)
    int arrowIdx = 0;

    auto placeArrow = [&](int cx)
    {
        if (arrowIdx < (int)arrowLabels.size() && arrowLabels[(size_t)arrowIdx] != nullptr)
        {
            arrowLabels[(size_t)arrowIdx]->setVisible(true);
            arrowLabels[(size_t)arrowIdx]->setBounds(cx, y + stripH / 2 - 18, arrowW, 36);
        }
        ++arrowIdx;
    };

    // First arrow (INPUT -> ...)
    placeArrow(x);
    x += arrowW + gap;

    // Visible plugin cards.
    for (int i = 0; i <= lastUsed; ++i)
    {
        auto* c = slotCards[(size_t)i].get();
        if (c == nullptr)
            continue;

        c->setBounds(x, y, pluginW, stripH);
        x += pluginW + gap;

        placeArrow(x);
        x += arrowW + gap;
    }

    // Pending add slot beyond lastUsed (rare)
    if (pendingAddSlotIndex > lastUsed && pendingAddSlotIndex >= 0 && pendingAddSlotIndex < kPluginChainMaxSlots)
    {
        auto* c = slotCards[(size_t)pendingAddSlotIndex].get();
        if (c != nullptr)
        {
            c->setBounds(x, y, pluginW, stripH);
            x += pluginW + gap;

            placeArrow(x);
            x += arrowW + gap;
        }
    }

    // Add card
    if (showAdd && addPluginCard != nullptr)
    {
        addPluginCard->setBounds(x, y, addW, stripH);
        x += addW + gap;

        placeArrow(x);
        x += arrowW + gap;
    }

    // Size content; center it in the viewport if it fits.
    const int contentW = x + pad;
    chainContent->setSize(juce::jmax(contentW, chainViewport.getWidth()), stripH + pad * 2);

    const int extra = chainViewport.getWidth() - contentW;
    if (extra > 0)
    {
        // Shift everything right to center.
        const int dx = extra / 2;
        for (auto& a : arrowLabels)
            if (a != nullptr && a->isVisible())
                a->setTopLeftPosition(a->getX() + dx, a->getY());

        for (int i = 0; i < kPluginChainMaxSlots; ++i)
            if (slotCards[(size_t)i] != nullptr && slotCards[(size_t)i]->isVisible())
                slotCards[(size_t)i]->setTopLeftPosition(slotCards[(size_t)i]->getX() + dx, slotCards[(size_t)i]->getY());

        if (addPluginCard != nullptr && addPluginCard->isVisible())
            addPluginCard->setTopLeftPosition(addPluginCard->getX() + dx, addPluginCard->getY());
    }
}

void RackViewComponent::timerCallback()
{
    refreshSlotDisplays();

    if (appContext.audioEngine != nullptr)
        globalBypassFxToggle.setToggleState(appContext.audioEngine->isGlobalBypass(), juce::dontSendNotification);

    // Playable Preset validation: confirm Slot 1 is actually processing blocks (no RT logging).
    static uint64_t lastSlot0ProcessCount = 0;
    if (appContext.pluginHostManager != nullptr)
        if (auto* chain = appContext.pluginHostManager->getPluginChain())
            if (auto* slot = chain->getSlot(static_cast<size_t>(0)))
            {
                const uint64_t n = slot->getProcessBlockCallCount();
                if (n != lastSlot0ProcessCount && n > 0 && (n % 200) == 0)
                    Logger::info("FORGE7 PlayablePreset: slot 0 processBlock calls=" + juce::String((juce::int64)n));
                lastSlot0ProcessCount = n;
            }
}

void RackViewComponent::refreshSlotDisplays()
{
    auto* chain = appContext.pluginHostManager->getPluginChain();

    if (chain == nullptr)
        return;

    compactChainLeadingGapsFromPluginLoads();

    const NavigationStatus nav = computeNavigationStatus(appContext);

    juce::String sceneLine =
        nav.hasActiveScene() ? "Scene " + juce::String(nav.sceneIndex + 1) + " - "
                                   + (nav.sceneName.isNotEmpty() ? nav.sceneName : juce::String("Untitled"))
                             : juce::String("Scene");

    const juce::String tempoLine =
        nav.hasActiveScene() ? juce::String(nav.tempoBpm, 1) + " BPM" : juce::String("- BPM");

    projectHeaderLabel.setText(nav.getProjectHeaderLine(), juce::dontSendNotification);

    if (appContext.projectSession != nullptr && appContext.projectSession->isProjectDirty())
    {
        projectDirtyLabel.setText("Unsaved changes", juce::dontSendNotification);
        projectDirtyLabel.setVisible(true);
    }
    else
    {
        projectDirtyLabel.setText({}, juce::dontSendNotification);
        projectDirtyLabel.setVisible(false);
    }

    sceneTitleLabel.setText(sceneLine, juce::dontSendNotification);
    chainHeaderLabel.setText(nav.getChainDisplayLabel(), juce::dontSendNotification);
    chainCountLabel.setText(nav.getChainCountSummary(), juce::dontSendNotification);
    tempoLabel.setText(tempoLine, juce::dontSendNotification);

    chainPrevButton.setEnabled(nav.chainCount > 0);
    chainNextButton.setEnabled(nav.chainCount > 0);
    addChainButton.setEnabled(appContext.sceneManager != nullptr);
    renameChainButton.setEnabled(nav.hasActiveChain());
    addSceneButton.setEnabled(appContext.sceneManager != nullptr);
    renameSceneButton.setEnabled(nav.hasActiveScene());

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        if (slotCards[static_cast<size_t>(i)] == nullptr)
            continue;

        auto info = chain->getSlotInfo(i);
        if (i == pendingAddSlotIndex && info.isEmpty && !info.missingPlugin && !info.isPlaceholder)
        {
            info.isPlaceholder = true;
            info.slotDisplayName = "+ Pending";
        }

        slotCards[static_cast<size_t>(i)]->refreshFromSlotInfo(info);
        slotCards[static_cast<size_t>(i)]->setSelected(selectedSlotIndex == i);
    }

    if (chainControlsPanel != nullptr)
        chainControlsPanel->refreshFromHost();

    // Context strip (selection-dependent labels / toggles)
    auto* chainForUi = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

    if (appContext.audioEngine != nullptr)
        globalBypassFxToggle.setToggleState(appContext.audioEngine->isGlobalBypass(), juce::dontSendNotification);

    const bool haveSlot =
        selectedSlotIndex >= 0 && selectedSlotIndex < kPluginChainMaxSlots && chainForUi != nullptr;
    SlotInfo ctxInfo {};

    if (haveSlot)
        ctxInfo = chainForUi->getSlotInfo(selectedSlotIndex);

    const bool hasBlock = haveSlot && (!ctxInfo.isEmpty || ctxInfo.isPlaceholder || ctxInfo.missingPlugin);

    ctxMoveLeftButton.setEnabled(haveSlot && selectedSlotIndex > 0);
    ctxMoveRightButton.setEnabled(haveSlot && selectedSlotIndex < kPluginChainMaxSlots - 1);
    ctxBypassToggle.setEnabled(hasBlock);

    if (hasBlock)
        ctxBypassToggle.setToggleState(ctxInfo.bypass, juce::dontSendNotification);
    else
        ctxBypassToggle.setToggleState(false, juce::dontSendNotification);

    ctxRemoveButton.setEnabled(hasBlock);
    ctxReplaceButton.setEnabled(haveSlot);
    ctxEditorButton.setEnabled(hasBlock);
    if (haveSlot)
        ctxReplaceButton.setButtonText((ctxInfo.isEmpty && !ctxInfo.missingPlugin) ? "Add" : "Replace");
    else
        ctxReplaceButton.setButtonText("Add");

    layoutRackChain();
}

void RackViewComponent::wireSlotCallbacks(const int slotIndex)
{
    auto& card = *slotCards[static_cast<size_t>(slotIndex)];

    card.onSelect = [this](int idx)
    {
        setSelectedSlot(idx);
    };

    card.onBypassChanged = [this](int idx, bool bypassed)
    {
        if (appContext.pluginHostManager == nullptr)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
            c->bypassSlot(idx, bypassed);

        if (appContext.projectSession != nullptr)
            appContext.projectSession->markProjectDirty();
    };

    card.onRemoveRequested = [this](int idx)
    {
        if (appContext.pluginHostManager == nullptr)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
            c->removePluginFromSlot(idx);

        if (appContext.projectSession != nullptr)
            appContext.projectSession->markProjectDirty();

        refreshSlotDisplays();
    };
}

void RackViewComponent::setSelectedSlot(const int slotIndex)
{
    selectedSlotIndex = slotIndex;

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
        if (slotCards[static_cast<size_t>(i)] != nullptr)
            slotCards[static_cast<size_t>(i)]->setSelected(i == selectedSlotIndex);

    // Details panel is chain-level (not slot-level); no per-slot selection binding.
}

void RackViewComponent::selectRackSlot(const int slotIndex)
{
    if (!juce::isPositiveAndBelow(slotIndex, kPluginChainMaxSlots))
        return;

    setSelectedSlot(slotIndex);
}

void RackViewComponent::refreshAfterProjectHydration()
{
    refreshSlotDisplays();
    syncEncoderFocus();
}

void RackViewComponent::showPluginBrowser()
{
    if (pluginBrowser == nullptr || browserOverlay == nullptr)
        return;

    /** Pick or confirm logical target slot before opening (Add / Replace vs generic picker). */
    if (browserOpenReason == BrowserOpenReason::ReplaceExistingSlot)
    {
        if (selectedSlotIndex < 0 || ! juce::isPositiveAndBelow(selectedSlotIndex, kPluginChainMaxSlots))
            return;
    }
    else if (browserOpenReason == BrowserOpenReason::AddNewSlot)
    {
        if (pendingAddSlotIndex >= 0 && juce::isPositiveAndBelow(pendingAddSlotIndex, kPluginChainMaxSlots))
            setSelectedSlot(pendingAddSlotIndex);
    }
    else if (selectedSlotIndex < 0)
    {
        const int emptySlot = findFirstEmptyBackendSlotIndex();

        if (emptySlot < 0)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                     "Chain full",
                                                     "All slots are in use. Remove or replace a plugin before adding "
                                                     "another.",
                                                     "OK");
            return;
        }

        setSelectedSlot(emptySlot);
    }

    juce::String reasonStr;

    switch (browserOpenReason)
    {
        case BrowserOpenReason::None:
            reasonStr = "None";
            break;
        case BrowserOpenReason::AddNewSlot:
            reasonStr = "AddNewSlot";
            break;
        case BrowserOpenReason::ReplaceExistingSlot:
            reasonStr = "ReplaceExistingSlot";
            break;
        default:
            reasonStr = "?";
            break;
    }

    const int tgt = resolveTargetSlotForPluginLoad();

    Logger::info("FORGE7 RackAdd: browser open reason=" + reasonStr + " targetSlot=" + juce::String(tgt)
                 + " selectedSlot=" + juce::String(selectedSlotIndex) + " pending=" + juce::String(pendingAddSlotIndex));

    pluginBrowser->rebuildList();

    browserOverlay->setBounds(getLocalBounds());
    browserOverlay->setVisible(true);
    addAndMakeVisible(*browserOverlay);
    browserOverlay->toFront(true);

    pluginBrowser->setBounds(browserOverlay->getLocalBounds().reduced(6));
    pluginBrowser->toFront(false);

    syncEncoderFocus();
}

void RackViewComponent::dismissPluginBrowserOverlay(const bool cancelled)
{
    /** Snapshot BEFORE mutating loading state (successful load clears `browserOpenReason` before dismissal). */
    const BrowserOpenReason snapReason = browserOpenReason;
    const int snapPending = pendingAddSlotIndex;
    const int snapSelBefore = selectionBeforePendingAdd;

    Logger::info("FORGE7 RackAdd: browser dismissed cancelled=" + juce::String(cancelled ? "true" : "false") + " snapReason="
                 + juce::String((int)snapReason) + " snapPendingSlot=" + juce::String(snapPending) + " selectedSlot="
                 + juce::String(selectedSlotIndex));

    if (browserOverlay != nullptr)
        browserOverlay->setVisible(false);

    if (cancelled && snapReason == BrowserOpenReason::AddNewSlot && snapPending >= 0
        && snapPending < kPluginChainMaxSlots)
    {
        auto* ch = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

        if (ch != nullptr)
        {
            const auto info = ch->getSlotInfo(snapPending);
            /** Pure-empty slot restore (browse cancelled without assign). */
            if (info.isEmpty && !info.missingPlugin && !info.isPlaceholder)
            {
                pendingAddSlotIndex = -1;
                selectionBeforePendingAdd = -1;
                setSelectedSlot(snapSelBefore);
                Logger::info("FORGE7 RackAdd: cancel restored selection to " + juce::String(snapSelBefore));
            }
        }
    }

    browserOpenReason = BrowserOpenReason::None;

    if (appContext.encoderNavigator != nullptr)
        appContext.encoderNavigator->clearModalFocusChain();

    refreshSlotDisplays();

    repaint();

    syncEncoderFocus();
}

bool RackViewComponent::isPluginBrowserVisible() const noexcept
{
    return browserOverlay != nullptr && browserOverlay->isVisible();
}

void RackViewComponent::closePluginBrowser()
{
    dismissPluginBrowserOverlay(true);
}

void RackViewComponent::syncEncoderFocus()
{
    if (appContext.encoderNavigator == nullptr || !isShowing())
        return;

    if (appContext.projectSceneJumpBrowserOpen)
        return;

    if (browserOverlay != nullptr && browserOverlay->isVisible() && pluginBrowser != nullptr)
    {
        pluginBrowser->ensureDefaultListSelectionForEncoder();
        appContext.encoderNavigator->setModalFocusChain(pluginBrowser->buildEncoderFocusItems());
        return;
    }

    appContext.encoderNavigator->clearModalFocusChain();

    std::vector<EncoderFocusItem> items;

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        if (slotCards[static_cast<size_t>(i)] == nullptr)
            continue;

        if (!slotCards[static_cast<size_t>(i)]->isVisible())
            continue;

        const int idx = i;
        items.push_back({ slotCards[static_cast<size_t>(i)].get(),
                         [this, idx]()
                         {
                             setSelectedSlot(idx);
                         },
                         {} });
    }

    if (addPluginCard != nullptr && addPluginCard->isVisible())
        items.push_back({ addPluginCard.get(), [this]() { if (addPluginCard != nullptr) addPluginCard->onAddClicked(); }, {} });

    items.push_back({ &chainPrevButton, [this]() { chainPrevButton.triggerClick(); }, {} });
    items.push_back({ &chainNextButton, [this]() { chainNextButton.triggerClick(); }, {} });
    items.push_back({ &addChainButton, [this]() { addChainButton.triggerClick(); }, {} });
    items.push_back({ &renameChainButton, [this]() { renameChainButton.triggerClick(); }, {} });
    items.push_back({ &addSceneButton, [this]() { addSceneButton.triggerClick(); }, {} });
    items.push_back({ &renameSceneButton, [this]() { renameSceneButton.triggerClick(); }, {} });

    items.push_back({ &ctxMoveLeftButton, [this]() { ctxMoveLeftButton.triggerClick(); }, {} });
    items.push_back({ &ctxMoveRightButton, [this]() { ctxMoveRightButton.triggerClick(); }, {} });
    items.push_back({ &ctxBypassToggle, [this]() { ctxBypassToggle.triggerClick(); }, {} });
    items.push_back({ &ctxRemoveButton, [this]() { ctxRemoveButton.triggerClick(); }, {} });
    items.push_back({ &ctxReplaceButton, [this]() { ctxReplaceButton.triggerClick(); }, {} });
    items.push_back({ &ctxEditorButton, [this]() { ctxEditorButton.triggerClick(); }, {} });
    items.push_back({ &globalBypassFxToggle, [this]() { globalBypassFxToggle.triggerClick(); }, {} });
    items.push_back({ &navPerformanceButton, [this]() { navPerformanceButton.triggerClick(); }, {} });
    items.push_back({ &settingsButton, [this]() { settingsButton.triggerClick(); }, {} });

    appContext.encoderNavigator->setRootFocusChain(std::move(items));
}

bool RackViewComponent::loadPluginIntoSelectedSlot(const juce::PluginDescription& desc)
{
    if (pluginBrowser != nullptr)
        pluginBrowser->clearLoadError();

    if (appContext.pluginHostManager == nullptr)
        return false;

    const int targetSlot = resolveTargetSlotForPluginLoad();

    if (! juce::isPositiveAndBelow(targetSlot, kPluginChainMaxSlots))
    {
        Logger::warn("FORGE7 RackAdd: invalid target slot resolved=" + juce::String(targetSlot));
        return false;
    }

    juce::String err;

    Logger::info("FORGE7 RackAdd: plugin chosen name=\"" + desc.name + "\" slot=" + juce::String(targetSlot)
                 + " reason=" + juce::String((int)browserOpenReason));

    Logger::info("FORGE7 PlayablePreset: RackView loadPluginIntoSelectedSlot slot=" + juce::String(targetSlot)
                 + " name=\"" + desc.name + "\" format=\"" + desc.pluginFormatName + "\"");

    if (! appContext.pluginHostManager->loadPluginIntoSlotSynchronously(desc, targetSlot, err))
    {
        const juce::String msg =
            err.isNotEmpty() ? err : juce::String("Could not load this plugin. Check install and format.");

        Logger::warn("FORGE7 RackAdd: load failed slot=" + juce::String(targetSlot) + " err=" + msg);

        if (pluginBrowser != nullptr)
            pluginBrowser->setLoadErrorMessage(msg);

        refreshSlotDisplays();
        return false;
    }

    Logger::info("FORGE7 PlayablePreset: plugin load+assign OK slot=" + juce::String(targetSlot));

    if (appContext.projectSession != nullptr)
        appContext.projectSession->markProjectDirty();

    pendingAddSlotIndex = -1;
    selectionBeforePendingAdd = -1;
    browserOpenReason = BrowserOpenReason::None;

    setSelectedSlot(targetSlot);

    const auto visSlots = getVisiblePluginSlotIndices();
    juce::String visStr;
    for (size_t v = 0; v < visSlots.size(); ++v)
    {
        visStr += juce::String(visSlots[v]);
        if (v + 1 < visSlots.size())
            visStr += ",";
    }

    const int nextAdd = findFirstEmptyBackendSlotIndex();

    Logger::info("FORGE7 RackAdd: load success slot=" + juce::String(targetSlot) + " visibleSlots=" + visStr
                 + " nextAddSlot=" + juce::String(nextAdd));

    refreshSlotDisplays();

    return true;
}

bool RackViewComponent::rackSlotShowsContent(const SlotInfo& info) noexcept
{
    return (!info.isEmpty) || info.missingPlugin || info.isPlaceholder;
}

bool RackViewComponent::chainSlotShowsInRailLine(const int pendingAddSlotIdx, const int slotIdx, const SlotInfo& info) noexcept
{
    if (pendingAddSlotIdx == slotIdx && info.isEmpty && !info.missingPlugin && !info.isPlaceholder)
        return true;

    return rackSlotShowsContent(info);
}

int RackViewComponent::resolveTargetSlotForPluginLoad() const noexcept
{
    if (browserOpenReason == BrowserOpenReason::AddNewSlot && pendingAddSlotIndex >= 0 && pendingAddSlotIndex < kPluginChainMaxSlots)
        return pendingAddSlotIndex;

    return selectedSlotIndex;
}

int RackViewComponent::findFirstEmptyBackendSlotIndex() const noexcept
{
    auto* chain = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

    if (chain == nullptr)
        return -1;

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        const auto info = chain->getSlotInfo(i);

        if (info.isEmpty && !info.missingPlugin && !info.isPlaceholder)
            return i;
    }

    return -1;
}

void RackViewComponent::compactChainLeadingGapsFromPluginLoads()
{
    if (isPluginBrowserVisible())
        return;

    auto* chain = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

    if (chain == nullptr)
        return;

    bool moved = true;
    int guard = 0;

    while (moved && guard++ < 64)
    {
        moved = false;

        for (int i = 1; i < kPluginChainMaxSlots; ++i)
        {
            const auto prevInfo = chain->getSlotInfo(i - 1);
            const auto currInfo = chain->getSlotInfo(i);

            const bool prevPureEmpty = prevInfo.isEmpty && !prevInfo.missingPlugin && !prevInfo.isPlaceholder;

            if (! prevPureEmpty)
                continue;

            if (! rackSlotShowsContent(currInfo))
                continue;

            if (chain->moveSlot(i, i - 1))
            {
                if (selectedSlotIndex == i)
                    selectedSlotIndex = i - 1;

                if (pendingAddSlotIndex == i)
                    pendingAddSlotIndex = i - 1;

                moved = true;
                break;
            }
        }
    }
}

void RackViewComponent::promptAddChain()
{
    if (appContext.sceneManager == nullptr)
        return;

    NameEntryModal::showPlainDialog(
        appContext,
        "New Chain",
        {},
        [this](const juce::String& enteredName)
        {
            if (appContext.sceneManager == nullptr)
                return;

            if (appContext.projectSession != nullptr)
                appContext.projectSession->captureLiveChainIntoModel();

            const juce::String trimmed = enteredName.trim();
            const juce::String id = appContext.sceneManager->createChainVariation(trimmed);

            if (id.isEmpty())
                Logger::warn("FORGE7: createChainVariation returned empty id");

            if (appContext.projectSession != nullptr)
                appContext.projectSession->pushActiveChainToLiveHost();
            else if (appContext.pluginHostManager != nullptr)
                appContext.pluginHostManager->commitChainVariationCrossfade(*appContext.sceneManager);

            refreshSlotDisplays();
            syncEncoderFocus();
        });
}

void RackViewComponent::promptRenameActiveChain()
{
    if (appContext.sceneManager == nullptr)
        return;

    auto* scene = appContext.sceneManager->getActiveScene();

    if (scene == nullptr)
        return;

    const int idx = scene->getActiveChainVariationIndex();
    auto& vars = scene->getVariations();

    if (! juce::isPositiveAndBelow(idx, static_cast<int>(vars.size())) || vars[static_cast<size_t>(idx)] == nullptr)
        return;

    const juce::String currentName = vars[static_cast<size_t>(idx)]->getVariationName();

    NameEntryModal::showPlainDialog(
        appContext,
        "Rename Chain",
        currentName,
        [this, idx](const juce::String& enteredName)
        {
            if (appContext.sceneManager == nullptr)
                return;

            appContext.sceneManager->renameChainVariation(idx, enteredName.trim());

            if (appContext.projectSession != nullptr)
                appContext.projectSession->markProjectDirty();

            refreshSlotDisplays();
            syncEncoderFocus();
        });
}

void RackViewComponent::promptAddScene()
{
    if (appContext.sceneManager == nullptr)
        return;

    NameEntryModal::showPlainDialog(
        appContext,
        "New Scene",
        {},
        [this](const juce::String& enteredName)
        {
            if (appContext.sceneManager == nullptr)
                return;

            if (appContext.projectSession != nullptr)
                appContext.projectSession->captureLiveChainIntoModel();

            const juce::String id = appContext.sceneManager->createScene(enteredName.trim());

            if (id.isEmpty())
                Logger::warn("FORGE7: createScene returned empty id");

            if (appContext.projectSession != nullptr)
                appContext.projectSession->pushActiveChainToLiveHost();
            else if (appContext.pluginHostManager != nullptr)
                appContext.pluginHostManager->commitChainVariationCrossfade(*appContext.sceneManager);

            refreshSlotDisplays();
            syncEncoderFocus();
        });
}

void RackViewComponent::promptRenameActiveScene()
{
    if (appContext.sceneManager == nullptr)
        return;

    const int idx = appContext.sceneManager->getActiveSceneIndex();
    auto* scene = appContext.sceneManager->getActiveScene();

    if (scene == nullptr)
        return;

    const juce::String currentName = scene->getSceneName();

    NameEntryModal::showPlainDialog(
        appContext,
        "Rename Scene",
        currentName,
        [this, idx](const juce::String& enteredName)
        {
            if (appContext.sceneManager == nullptr)
                return;

            appContext.sceneManager->renameScene(idx, enteredName.trim());

            if (appContext.projectSession != nullptr)
                appContext.projectSession->markProjectDirty();

            refreshSlotDisplays();
            syncEncoderFocus();
        });
}

std::vector<int> RackViewComponent::getVisiblePluginSlotIndices() const
{
    std::vector<int> out;

    auto* chain = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

    if (chain == nullptr)
        return out;

    int lastUsed = -1;

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        const auto info = chain->getSlotInfo(i);

        if (chainSlotShowsInRailLine(pendingAddSlotIndex, i, info))
            lastUsed = i;
        else
            break;
    }

    for (int i = 0; i <= lastUsed; ++i)
        out.push_back(i);

    return out;
}

} // namespace forge7
