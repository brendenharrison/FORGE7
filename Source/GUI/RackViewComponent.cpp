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
#include "../Scene/SceneManager.h"
#include "../Utilities/Logger.h"
#include "CpuMeter.h"
#include "PluginBrowserComponent.h"
#include "PluginInspectorComponent.h"
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

    sceneTitleLabel.setJustificationType(juce::Justification::centredLeft);
    sceneTitleLabel.setFont(juce::Font(17.0f));
    sceneTitleLabel.setColour(juce::Label::textColourId, rackText());
    addAndMakeVisible(sceneTitleLabel);

    variationLabel.setJustificationType(juce::Justification::centredLeft);
    variationLabel.setFont(juce::Font(15.0f));
    variationLabel.setColour(juce::Label::textColourId, rackMuted());
    addAndMakeVisible(variationLabel);

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
    };

    globalBypassFxToggle.setColour(juce::ToggleButton::textColourId, rackText());
    globalBypassFxToggle.setColour(juce::ToggleButton::tickColourId, rackAccent());
    addAndMakeVisible(globalBypassFxToggle);

    chainContent = std::make_unique<juce::Component>();
    chainContent->setInterceptsMouseClicks(true, true);

    inputBlock = std::make_unique<IoBlock>(IoBlock::Kind::Input);
    outputBlock = std::make_unique<IoBlock>(IoBlock::Kind::Output);
    chainContent->addAndMakeVisible(*inputBlock);
    chainContent->addAndMakeVisible(*outputBlock);

    for (auto& a : arrowLabels)
    {
        a = std::make_unique<juce::Label>();
        a->setText(">", juce::dontSendNotification);
        a->setJustificationType(juce::Justification::centred);
        a->setColour(juce::Label::textColourId, rackMuted());
        a->setFont(juce::Font(28.0f));
        a->setInterceptsMouseClicks(false, false);
        chainContent->addAndMakeVisible(*a);
    }

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        slotCards[static_cast<size_t>(i)] = std::make_unique<RackSlotCard>();
        slotCards[static_cast<size_t>(i)]->setSlotIndex(i);
        slotCards[static_cast<size_t>(i)]->setShowInlineControls(false);
        wireSlotCallbacks(i);
        chainContent->addAndMakeVisible(*slotCards[static_cast<size_t>(i)]);
    }

    chainViewport.setViewedComponent(chainContent.get(), false);
    chainViewport.setScrollBarsShown(true, false);
    chainViewport.getHorizontalScrollBar().setColour(juce::ScrollBar::thumbColourId, rackAccent().withAlpha(0.55f));
    addAndMakeVisible(chainViewport);

    navPerformanceButton.setButtonText("Performance");
    navScenesButton.setButtonText("Scenes");

    for (auto* b : { &navPerformanceButton, &navScenesButton })
    {
        styleBottomNavButton(*b);
        b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible(*b);
    }

    navPerformanceButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->setEditMode(false);
    };

    navScenesButton.onClick = []()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                 "Scenes",
                                                 "Scene browser coming soon.",
                                                 "OK");
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
    };
    addAndMakeVisible(ctxBypassToggle);

    wireCtxText(ctxMoveLeftButton);
    wireCtxText(ctxMoveRightButton);
    wireCtxText(ctxRemoveButton);
    wireCtxText(ctxReplaceButton);
    wireCtxText(ctxEditorButton);
    wireCtxText(ctxDetailButton);

    ctxRemoveButton.onClick = [this]()
    {
        if (appContext.pluginHostManager == nullptr || selectedSlotIndex < 0)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
            c->removePluginFromSlot(selectedSlotIndex);

        refreshSlotDisplays();
    };

    ctxReplaceButton.onClick = [this]() { showPluginBrowser(); };

    ctxEditorButton.onClick = [this]()
    {
        if (selectedSlotIndex < 0)
            return;

        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->openFullscreenPluginEditor(selectedSlotIndex);
    };

    ctxDetailButton.onClick = [this]()
    {
        inspectorExpanded = !inspectorExpanded;

        if (pluginInspector != nullptr)
        {
            pluginInspector->setVisible(inspectorExpanded);
            ctxDetailButton.setButtonText(inspectorExpanded ? "Hide" : "Details");
        }

        resized();
        syncEncoderFocus();
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

        refreshSlotDisplays();
    };

    settingsButton.onClick = []()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                 "Settings",
                                                 "App settings - coming soon.",
                                                 "OK");
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
            hidePluginBrowser();
    };

    pluginBrowser->onDismiss = [this]()
    {
        hidePluginBrowser();
    };

    browserOverlay->addAndMakeVisible(*pluginBrowser);

    pluginBrowser->setInterceptsMouseClicks(true, true);
    pluginBrowser->toFront(false);

    pluginInspector = std::make_unique<PluginInspectorComponent>(appContext);
    pluginInspector->onModelChanged = [this]()
    {
        refreshSlotDisplays();
        syncEncoderFocus();
    };
    pluginInspector->onMappingsChanged = [this]()
    {
        refreshSlotDisplays();
    };
    pluginInspector->setVisible(false);

    addChildComponent(*pluginInspector);

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

    const int statusH = 52;
    const int contextH = 50;
    const int bottomH = 52;
    const int pad = 8;

    // Top: scene / variation / edit badge / tempo / global bypass / CPU
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

    const int varW = juce::jmin(200, juce::jmax(100, statusArea.getWidth() / 4));
    variationLabel.setBounds(statusArea.removeFromRight(varW));
    statusArea.removeFromRight(6);
    sceneTitleLabel.setBounds(statusArea);

    area.removeFromTop(4);

    // Bottom: primary nav (Performance / Scenes) + settings
    auto bottomArea = area.removeFromBottom(bottomH).reduced(pad, 4);

    settingsButton.setBounds(bottomArea.removeFromRight(100).reduced(0, 2));
    bottomArea.removeFromRight(8);

    const int navHalf = juce::jmax(120, bottomArea.getWidth() / 2);
    navPerformanceButton.setBounds(bottomArea.removeFromLeft(navHalf).reduced(4, 0));
    navScenesButton.setBounds(bottomArea.reduced(4, 0));

    area.removeFromBottom(4);

    // Slot actions (hardware-style strip)
    auto ctxRow = area.removeFromBottom(contextH).reduced(pad, 2);
    const int ctxGap = 6;

    ctxDetailButton.setBounds(ctxRow.removeFromRight(92).reduced(0, 2));
    ctxRow.removeFromRight(ctxGap);
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

    const int inspectorH = inspectorExpanded ? juce::jlimit(140, 220, juce::roundToInt(0.28f * (float)getHeight()))
                                             : 0;

    if (inspectorExpanded && pluginInspector != nullptr)
        pluginInspector->setBounds(area.removeFromBottom(inspectorH).reduced(pad, 0));
    else if (pluginInspector != nullptr)
        pluginInspector->setBounds({});

    area.removeFromBottom(inspectorExpanded ? 4 : 0);

    chainViewport.setBounds(area);

    layoutRackChain();

    syncEncoderFocus();
}

void RackViewComponent::layoutRackChain()
{
    if (chainContent == nullptr)
        return;

    const int ioW = 80;
    const int arrowW = 28;
    const int gap = 8;
    const int pad = 10;
    const int stripH = juce::jmax(140, chainViewport.getHeight() > 0 ? chainViewport.getHeight() - 12 : 140);

    const int availForSlots = juce::jmax(88 * kPluginChainMaxSlots,
                                         chainViewport.getWidth() - (pad * 2) - ioW * 2 - arrowW * 9 - gap * 18);

    const int slotW = juce::jmax(88, availForSlots / kPluginChainMaxSlots);

    int x = pad;
    const int y = pad;

    inputBlock->setBounds(x, y, ioW, stripH);
    x += ioW + gap;

    for (int i = 0; i < 8; ++i)
    {
        arrowLabels[static_cast<size_t>(i)]->setBounds(x, y + stripH / 2 - 18, arrowW, 36);
        x += arrowW + gap;

        slotCards[static_cast<size_t>(i)]->setBounds(x, y, slotW, stripH);
        x += slotW + gap;
    }

    arrowLabels[8]->setBounds(x, y + stripH / 2 - 18, arrowW, 36);
    x += arrowW + gap;

    outputBlock->setBounds(x, y, ioW, stripH);

    const int totalW = x + ioW + pad;
    chainContent->setSize(totalW, stripH + pad * 2);
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

    juce::String sceneLine = "Scene";
    juce::String varLine = "Variation";
    juce::String tempoLine = "- BPM";

    if (appContext.sceneManager != nullptr)
    {
        const int si = appContext.sceneManager->getActiveSceneIndex();
        const auto& scenes = appContext.sceneManager->getScenes();

        if (juce::isPositiveAndBelow(si, static_cast<int>(scenes.size())) && scenes[static_cast<size_t>(si)] != nullptr)
        {
            const auto& sc = *scenes[static_cast<size_t>(si)];
            sceneLine = "Scene " + juce::String(si + 1) + " - " + sc.getSceneName();

            const auto& vars = sc.getVariations();

            const int vi =
                vars.empty() ? 0
                             : juce::jlimit(0,
                                            static_cast<int>(vars.size()) - 1,
                                            sc.getActiveChainVariationIndex());

            if (juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) && vars[static_cast<size_t>(vi)] != nullptr)
                varLine = vars[static_cast<size_t>(vi)]->getVariationName();

            tempoLine = juce::String(sc.getTempoBpm(), 1) + " BPM";
        }
    }

    sceneTitleLabel.setText(sceneLine, juce::dontSendNotification);
    variationLabel.setText(varLine, juce::dontSendNotification);
    tempoLabel.setText(tempoLine, juce::dontSendNotification);

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        if (slotCards[static_cast<size_t>(i)] == nullptr)
            continue;

        slotCards[static_cast<size_t>(i)]->refreshFromSlotInfo(chain->getSlotInfo(i));
        slotCards[static_cast<size_t>(i)]->setSelected(selectedSlotIndex == i);
    }

    if (pluginInspector != nullptr)
        pluginInspector->refreshFromHost();

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
    ctxDetailButton.setEnabled(hasBlock);

    if (haveSlot)
        ctxReplaceButton.setButtonText((ctxInfo.isEmpty && !ctxInfo.missingPlugin) ? "Add" : "Replace");
    else
        ctxReplaceButton.setButtonText("Add");
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
    };

    card.onRemoveRequested = [this](int idx)
    {
        if (appContext.pluginHostManager == nullptr)
            return;

        if (auto* c = appContext.pluginHostManager->getPluginChain())
            c->removePluginFromSlot(idx);

        refreshSlotDisplays();
    };
}

void RackViewComponent::setSelectedSlot(const int slotIndex)
{
    selectedSlotIndex = slotIndex;

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
        if (slotCards[static_cast<size_t>(i)] != nullptr)
            slotCards[static_cast<size_t>(i)]->setSelected(i == selectedSlotIndex);

    if (pluginInspector != nullptr)
        pluginInspector->setInspectedSlot(selectedSlotIndex);
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
    if (selectedSlotIndex < 0)
    {
        auto* chain = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

        if (chain == nullptr)
            return;

        int emptySlot = -1;

        for (int i = 0; i < kPluginChainMaxSlots; ++i)
        {
            const auto info = chain->getSlotInfo(i);

            /** V1: treat "empty" as no loaded plugin; placeholders still occupy the slot. */
            if (info.isEmpty && !info.missingPlugin)
            {
                emptySlot = i;
                break;
            }
        }

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

    pluginBrowser->rebuildList();

    browserOverlay->setBounds(getLocalBounds());
    browserOverlay->setVisible(true);
    addAndMakeVisible(*browserOverlay);
    browserOverlay->toFront(true);

    pluginBrowser->setBounds(browserOverlay->getLocalBounds().reduced(6));
    pluginBrowser->toFront(false);

    syncEncoderFocus();
}

void RackViewComponent::hidePluginBrowser()
{
    if (browserOverlay != nullptr)
        browserOverlay->setVisible(false);

    if (appContext.encoderNavigator != nullptr)
        appContext.encoderNavigator->clearModalFocusChain();

    syncEncoderFocus();
}

bool RackViewComponent::isPluginBrowserVisible() const noexcept
{
    return browserOverlay != nullptr && browserOverlay->isVisible();
}

void RackViewComponent::closePluginBrowser()
{
    hidePluginBrowser();
}

void RackViewComponent::syncEncoderFocus()
{
    if (appContext.encoderNavigator == nullptr || !isShowing())
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

        const int idx = i;
        items.push_back({ slotCards[static_cast<size_t>(i)].get(),
                         [this, idx]()
                         {
                             setSelectedSlot(idx);
                         },
                         {} });
    }

    items.push_back({ &ctxMoveLeftButton, [this]() { ctxMoveLeftButton.triggerClick(); }, {} });
    items.push_back({ &ctxMoveRightButton, [this]() { ctxMoveRightButton.triggerClick(); }, {} });
    items.push_back({ &ctxBypassToggle, [this]() { ctxBypassToggle.triggerClick(); }, {} });
    items.push_back({ &ctxRemoveButton, [this]() { ctxRemoveButton.triggerClick(); }, {} });
    items.push_back({ &ctxReplaceButton, [this]() { ctxReplaceButton.triggerClick(); }, {} });
    items.push_back({ &ctxEditorButton, [this]() { ctxEditorButton.triggerClick(); }, {} });
    items.push_back({ &ctxDetailButton, [this]() { ctxDetailButton.triggerClick(); }, {} });
    items.push_back({ &globalBypassFxToggle, [this]() { globalBypassFxToggle.triggerClick(); }, {} });
    items.push_back({ &navPerformanceButton, [this]() { navPerformanceButton.triggerClick(); }, {} });
    items.push_back({ &navScenesButton, [this]() { navScenesButton.triggerClick(); }, {} });
    items.push_back({ &settingsButton, [this]() { settingsButton.triggerClick(); }, {} });

    appContext.encoderNavigator->setRootFocusChain(std::move(items));
}

bool RackViewComponent::loadPluginIntoSelectedSlot(const juce::PluginDescription& desc)
{
    if (pluginBrowser != nullptr)
        pluginBrowser->clearLoadError();

    if (appContext.pluginHostManager == nullptr || selectedSlotIndex < 0)
        return false;

    juce::String err;

    Logger::info("FORGE7 PlayablePreset: RackView loadPluginIntoSelectedSlot slot=" + juce::String(selectedSlotIndex)
                 + " name=\"" + desc.name + "\" format=\"" + desc.pluginFormatName + "\"");

    if (appContext.pluginHostManager->loadPluginIntoSlotSynchronously(desc, selectedSlotIndex, err))
    {
        Logger::info("FORGE7 PlayablePreset: plugin load+assign OK slot=" + juce::String(selectedSlotIndex));
        refreshSlotDisplays();
        return true;
    }

    const juce::String msg =
        err.isNotEmpty() ? err : juce::String("Could not load this plugin. Check install and format.");

    Logger::warn("FORGE7 PlayablePreset: plugin load failed slot=" + juce::String(selectedSlotIndex) + " err=" + msg);

    if (pluginBrowser != nullptr)
        pluginBrowser->setLoadErrorMessage(msg);

    refreshSlotDisplays();
    return false;
}

} // namespace forge7
