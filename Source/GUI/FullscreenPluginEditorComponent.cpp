#include "FullscreenPluginEditorComponent.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include "../App/AppContext.h"
#include "../Audio/AudioEngine.h"
#include "CpuMeter.h"
#include "../Controls/EncoderFocusTypes.h"
#include "../Controls/EncoderNavigator.h"
#include "../Controls/HardwareControlTypes.h"
#include "../Controls/ParameterMappingManager.h"
#include "HardwareAssignableUi.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{
juce::Colour bg() noexcept { return juce::Colour(0xff0d0f12); }
juce::Colour panel() noexcept { return juce::Colour(0xff161a20); }
juce::Colour text() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour muted() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour accent() noexcept { return juce::Colour(0xff4a9eff); }

juce::String mappingLabelForKnob(const juce::Array<ParameterMappingDescriptor>& rows,
                                 const juce::String& sceneId,
                                 const juce::String& varId,
                                 const int slotIndex,
                                 const HardwareControlId hid)
{
    for (const auto& m : rows)
    {
        if (m.sceneId == sceneId && m.chainVariationId == varId && m.pluginSlotIndex == slotIndex
            && m.hardwareControlId == hid)
            return m.displayName.isNotEmpty() ? m.displayName : juce::String("Param");
    }

    return juce::String("-");
}
} // namespace

struct FullscreenPluginEditorComponent::DeferredEditorHostReconciler final : juce::AsyncUpdater
{
    explicit DeferredEditorHostReconciler(FullscreenPluginEditorComponent& o) noexcept : owner(o) {}

    void handleAsyncUpdate() override { owner.performDeferredEditorHostReconcile(); }

    FullscreenPluginEditorComponent& owner;
};

FullscreenPluginEditorComponent::FullscreenPluginEditorComponent(AppContext& context,
                                                                  const int pluginSlotIndex,
                                                                  std::function<void()> onCloseRequested)
    : appContext(context)
    , slotIndex(pluginSlotIndex)
    , onClose(std::move(onCloseRequested))
    , headerChromeBg(bg())
    , footerChromeBg(bg())
    , parameterList({}, static_cast<juce::ListBoxModel*>(this))
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    setOpaque(true);

    deferredEditorHostReconciler = std::make_unique<DeferredEditorHostReconciler>(*this);

    addAndMakeVisible(pluginViewportFrame);
    pluginViewportFrame.addAndMakeVisible(pluginEditorCanvas);

    // V1 default: ActualSize. Vendor GUI keeps its natural size; oversized editors are reachable via scrollbars.
    pluginEditorCanvas.setPanMode(false);
    pluginEditorCanvas.resetPluginViewToActualSize();

    addAndMakeVisible(headerChromeBg);
    addAndMakeVisible(footerChromeBg);

    backButton.onClick = [this]()
    {
        if (onClose != nullptr)
            onClose();
    };

    addAndMakeVisible(backButton);

    closeButton.onClick = [this]()
    {
        if (onClose != nullptr)
            onClose();
    };

    addAndMakeVisible(closeButton);

    if (appContext.audioEngine != nullptr)
    {
        cpuMeter = std::make_unique<CpuMeter>(appContext.audioEngine);
        addAndMakeVisible(*cpuMeter);
    }

    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(18.0f));
    titleLabel.setColour(juce::Label::textColourId, text());
    addAndMakeVisible(titleLabel);

    sceneVarLabel.setJustificationType(juce::Justification::centredRight);
    sceneVarLabel.setFont(juce::Font(14.0f));
    sceneVarLabel.setColour(juce::Label::textColourId, muted());
    addAndMakeVisible(sceneVarLabel);

    assignModeToggle.setColour(juce::ToggleButton::textColourId, text());
    assignModeToggle.setColour(juce::ToggleButton::tickColourId, accent());
    assignModeToggle.onClick = [this]()
    {
        // Visibility of the assign hint + parameter list is owned by `resized()` (right inspector strip).
        resized();
        syncEncoderFocus();

        if (appContext.parameterMappingManager != nullptr && !assignModeToggle.getToggleState())
            appContext.parameterMappingManager->cancelKnobAssignmentLearn();
    };
    addAndMakeVisible(assignModeToggle);

    assignHintLabel.setText(
        "Select a parameter, then twist K1-K4 to assign (or press Button 1 / Button 2 for toggles).",
        juce::dontSendNotification);
    assignHintLabel.setFont(juce::Font(13.0f));
    assignHintLabel.setColour(juce::Label::textColourId, muted());
    assignHintLabel.setJustificationType(juce::Justification::centredLeft);
    assignHintLabel.setVisible(false);
    addAndMakeVisible(assignHintLabel);

    parameterList.setRowHeight(40);
    parameterList.setColour(juce::ListBox::backgroundColourId, panel().brighter(0.04f));
    parameterList.setMultipleSelectionEnabled(false);
    parameterList.setVisible(false);
    addAndMakeVisible(parameterList);

    for (auto& l : knobMappingLabels)
    {
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(13.0f));
        l.setColour(juce::Label::textColourId, text());
        addAndMakeVisible(l);
    }

    for (size_t i = 0; i < assignMappingLabels.size(); ++i)
    {
        assignMappingLabels[i].setJustificationType(juce::Justification::centredLeft);
        assignMappingLabels[i].setFont(juce::Font(13.0f));
        assignMappingLabels[i].setColour(juce::Label::textColourId,
                                          i == 0 ? button1Colour() : button2Colour());
        addAndMakeVisible(assignMappingLabels[i]);
    }

    // Scrollbar-style sliders: compact, no text box, no labels. 0..1 normalized scroll position
    // (0 = leftmost/topmost, 1 = rightmost/bottommost) - feels like a web browser scrollbar.
    panXSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panXSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panXSlider.setRange(0.0, 1.0, 0.0);
    panXSlider.setEnabled(false);
    panXSlider.onValueChange = [this]()
    {
        pluginEditorCanvas.setScrollX01(static_cast<float>(panXSlider.getValue()));
    };
    addAndMakeVisible(panXSlider);

    panYSlider.setSliderStyle(juce::Slider::LinearVertical);
    panYSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panYSlider.setRange(0.0, 1.0, 0.0);
    panYSlider.setEnabled(false);
    // JUCE vertical linear: smaller value nearer bottom of track. Map so bottom = scrollY 1 (browser-like).
    panYSlider.onValueChange = [this]()
    {
        const float v = static_cast<float>(panYSlider.getValue());
        pluginEditorCanvas.setScrollY01(1.0f - v);
    };
    addAndMakeVisible(panYSlider);

    if (appContext.pluginHostManager != nullptr)
        if (auto* chain = appContext.pluginHostManager->getPluginChain())
            if (auto* slot = chain->getSlot(static_cast<size_t>(slotIndex)))
                hostedInstanceForEditor = slot->getHostedInstance();

    refreshPanControlsFromCanvas();

    rebuildParameterListModel();

    refreshMappingStrip();
    syncEncoderFocus();
    bringChromeToFront();
    scheduleDeferredEditorHostReconcile();
    startTimerHz(8);
}

FullscreenPluginEditorComponent::~FullscreenPluginEditorComponent()
{
    stopTimer();

    if (deferredEditorHostReconciler != nullptr)
        deferredEditorHostReconciler->cancelPendingUpdate();

    deferredEditorHostReconciler.reset();

    if (appContext.parameterMappingManager != nullptr)
        appContext.parameterMappingManager->cancelKnobAssignmentLearn();

    pluginEditorCanvas.clearHostedEditor();
    embeddedEditor.reset();
}

void FullscreenPluginEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(bg());
}

void FullscreenPluginEditorComponent::timerCallback()
{
    refreshMappingStrip();
    refreshPanControlsFromCanvas();

    if (appContext.sceneManager != nullptr)
    {
        const int si = appContext.sceneManager->getActiveSceneIndex();
        const auto& scenes = appContext.sceneManager->getScenes();

        juce::String line = "Scene";

        if (juce::isPositiveAndBelow(si, static_cast<int>(scenes.size())) && scenes[static_cast<size_t>(si)] != nullptr)
        {
            const auto& sc = *scenes[static_cast<size_t>(si)];
            line = "Scene " + juce::String(si + 1) + " - " + sc.getSceneName();

            const auto& vars = sc.getVariations();
            const int vi =
                vars.empty()
                    ? 0
                    : juce::jlimit(0, static_cast<int>(vars.size()) - 1, sc.getActiveChainVariationIndex());

            if (juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) && vars[static_cast<size_t>(vi)] != nullptr)
            {
                const int oneBased = vi + 1;
                const juce::String idx = oneBased < 10 ? juce::String("0") + juce::String(oneBased)
                                                       : juce::String(oneBased);
                const juce::String name = vars[static_cast<size_t>(vi)]->getVariationName();
                line += "   -   Chain " + idx;
                if (name.isNotEmpty())
                    line += " - " + name;
            }
        }

        sceneVarLabel.setText(line, juce::dontSendNotification);
    }
}

void FullscreenPluginEditorComponent::resized()
{
    // Layout-first solution to the "native plugin surface paints over Forge controls" bug:
    //
    //   1. Header / footer / Assign inspector / scrollbars are sibling rectangles of the plugin
    //      viewport. None of them intersect `currentPluginViewportBounds`, so the hosted editor's
    //      native child window cannot end up geometrically over any host control.
    //   2. The footer height is constant - it never grows when Assign turns on. Variable-height
    //      footers used to share an edge with the plugin viewport, which is the worst-case region
    //      for native bleed (HWND/NSView overdraw, OpenGL surfaces, etc.).
    //   3. Assign mode reveals a right-side inspector strip outside the viewport instead, so the
    //      plugin viewport simply shrinks horizontally.
    auto area = getLocalBounds().reduced(10, 8);

    constexpr int kHeaderH = 48;
    constexpr int kFooterH = 108;
    constexpr int kAssignInspectorW = 320;
    constexpr int kScrollbarThickness = 20;
    constexpr int kHeaderToWorkspaceGap = 6;
    constexpr int kInspectorToViewportGap = 8;

    const auto headerBand = area.removeFromTop(kHeaderH);
    headerChromeBg.setBounds(headerBand);

    auto top = headerBand;
    backButton.setBounds(top.removeFromLeft(80).reduced(0, 4));
    top.removeFromLeft(8);
    assignModeToggle.setBounds(top.removeFromRight(142).reduced(0, 4));
    top.removeFromRight(8);

    if (cpuMeter != nullptr)
    {
        cpuMeter->setBounds(top.removeFromRight(92).reduced(0, 4));
        top.removeFromRight(8);
    }

    const int sceneW = juce::jmin(260, juce::jmax(160, top.getWidth() / 2));
    sceneVarLabel.setBounds(top.removeFromRight(sceneW).reduced(0, 2));
    top.removeFromRight(8);
    titleLabel.setBounds(top);

    area.removeFromTop(kHeaderToWorkspaceGap);

    // Fixed-height footer band - host chrome anchors here regardless of Assign mode.
    const auto footerBand = area.removeFromBottom(kFooterH);
    footerChromeBg.setBounds(footerBand);

    auto footer = footerBand;
    auto strip = footer.removeFromBottom(72).reduced(0, 4);
    closeButton.setBounds(strip.removeFromRight(96).reduced(4, 4));
    strip.removeFromRight(6);

    const int sixth = juce::jmax(72, strip.getWidth() / 6);

    for (int i = 0; i < 4; ++i)
        knobMappingLabels[static_cast<size_t>(i)].setBounds(strip.removeFromLeft(sixth).reduced(4, 0));

    for (int i = 0; i < 2; ++i)
        assignMappingLabels[static_cast<size_t>(i)].setBounds(strip.removeFromLeft(sixth).reduced(4, 0));

    // Right-side Assign inspector. Sibling of the plugin viewport, never on top of it.
    const bool assignActive = assignModeToggle.getToggleState();

    if (assignActive)
    {
        auto inspector = area.removeFromRight(kAssignInspectorW);
        inspector.removeFromLeft(kInspectorToViewportGap);

        assignHintLabel.setBounds(inspector.removeFromTop(22));
        inspector.removeFromTop(4);
        parameterList.setBounds(inspector);
    }

    assignHintLabel.setVisible(assignActive);
    parameterList.setVisible(assignActive);

    // Horizontal scrollbar directly above the footer; vertical along the right of the workspace.
    const auto hScrollBand = area.removeFromBottom(kScrollbarThickness);
    panXSlider.setBounds(hScrollBand);

    auto pluginWorkspace = area;
    const auto vScrollBand = pluginWorkspace.removeFromRight(kScrollbarThickness);
    panYSlider.setBounds(vScrollBand);

    // Guardrail: never collapse the plugin viewport to <1x1 - some plugin peers crash on 0-sized parents.
    currentPluginViewportBounds = pluginWorkspace;
    const auto safeViewport = currentPluginViewportBounds.withSize(juce::jmax(1, currentPluginViewportBounds.getWidth()),
                                                                    juce::jmax(1, currentPluginViewportBounds.getHeight()));

    pluginViewportFrame.setBounds(safeViewport);
    pluginEditorCanvas.setBounds(pluginViewportFrame.getLocalBounds());

    scheduleDeferredEditorHostReconcile();
    refreshPanControlsFromCanvas();

    // With non-overlapping geometry, JUCE z-order between sibling chrome components is irrelevant
    // for correctness; we still nudge the viewport frame toBack() so any future near-edge chrome
    // paints above it without a flicker. Crucially, we no longer rely on toFront() to beat native
    // plugin surfaces - that approach does not work for HWND / NSView / X11 plugin children.
    pluginViewportFrame.toBack();
}

void FullscreenPluginEditorComponent::bringChromeToFront()
{
    // The production fix is layout-based: chrome rectangles do not intersect the plugin viewport,
    // so JUCE z-order between sibling components is sufficient for paint correctness. We keep this
    // helper as a thin nudge that pushes the viewport frame toBack() (no toFront() spam) - native
    // plugin peers will draw above their JUCE parent regardless of toFront() / toBack(), which is
    // why geometry must not overlap in the first place.
    pluginViewportFrame.toBack();
}

void FullscreenPluginEditorComponent::scheduleDeferredEditorHostReconcile()
{
    if (deferredEditorHostReconciler != nullptr)
        deferredEditorHostReconciler->triggerAsyncUpdate();
}

void FullscreenPluginEditorComponent::performDeferredEditorHostReconcile()
{
    const bool attachedNow = tryAttachEmbeddedEditorIfNeeded();

    if (attachedNow)
        pluginEditorCanvas.reattachHostedEditorIfPresent();

    logPluginEditorLayoutDiagnosticsIfChanged();
    bringChromeToFront();

#if FORGE7_EXPERIMENTAL_NATIVE_PLUGIN_OVERLAY
    // Off by default. Intentional no-op stub for an experimental platform-specific heavyweight
    // overlay (e.g. NSWindow / HWND child added above the hosted plugin's native surface for
    // temporary debug HUDs). The production fix is layout-based; do not rely on this flag in
    // shipped builds. Kept here as the integration point if a debug-only experiment is needed.
    static_assert(true, "FORGE7_EXPERIMENTAL_NATIVE_PLUGIN_OVERLAY enabled but no stub implementation provided");
#endif
}

bool FullscreenPluginEditorComponent::tryAttachEmbeddedEditorIfNeeded()
{
    if (embeddedEditor != nullptr || hostedInstanceForEditor == nullptr)
        return false;

    // Wait until the viewport has real size; on first layout `resized()` may run before the
    // window has its final bounds. Attaching at <12px would just churn pluginEditor->setSize(0,0)
    // for some peers (guardrail against resize loops + degenerate bounds).
    if (pluginViewportFrame.getWidth() < 12 || pluginViewportFrame.getHeight() < 12)
        return false;

    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (hostedInstanceForEditor->hasEditor())
        embeddedEditor.reset(hostedInstanceForEditor->createEditorIfNeeded());

    if (embeddedEditor == nullptr)
        embeddedEditor = std::make_unique<juce::GenericAudioProcessorEditor>(*hostedInstanceForEditor);

    // Inform the canvas of plugin format / display name before attaching; this drives the
    // conservative `PluginEditorSizingMode` defaults (e.g. legacy VST2 stays NativeSizeCentered).
    pluginEditorCanvas.setPluginDescriptionForSizing(hostedInstanceForEditor->getPluginDescription());
    pluginEditorCanvas.setHostedEditor(embeddedEditor.get());
    refreshPanControlsFromCanvas();

    DBG("FORGE7 FullscreenPlugin: attached editor"
        << " plugin=" << hostedInstanceForEditor->getPluginDescription().name
        << " format=" << hostedInstanceForEditor->getPluginDescription().pluginFormatName
        << " naturalSize=" << pluginEditorCanvas.getNaturalEditorWidth()
        << "x" << pluginEditorCanvas.getNaturalEditorHeight()
        << " viewport=" << currentPluginViewportBounds.toString()
        << " sizingMode=" << static_cast<int>(pluginEditorCanvas.getSizingMode())
        << " policyAllowsResize=" << (pluginEditorCanvas.isResizableUnderCurrentPolicy() ? "yes" : "no"));

    return true;
}

void FullscreenPluginEditorComponent::logPluginEditorLayoutDiagnosticsIfChanged()
{
    if (embeddedEditor == nullptr)
        return;

    const auto vp = pluginViewportFrame.getBounds();

    if (vp == lastPluginLayoutDiagnosticBounds)
        return;

    lastPluginLayoutDiagnosticBounds = vp;

    juce::String modeStr;

    switch (pluginEditorCanvas.getViewMode())
    {
        case PluginEditorCanvas::PluginEditorViewMode::ActualSize:
            modeStr = "Actual";
            break;
        case PluginEditorCanvas::PluginEditorViewMode::FitToScreen:
            modeStr = "Fit";
            break;
        case PluginEditorCanvas::PluginEditorViewMode::FitWidth:
            modeStr = "Width";
            break;
        default:
            modeStr = "?";
            break;
    }

    const auto editorInCanvas = pluginEditorCanvas.getHostedEditorBoundsInCanvas();

    const juce::String diag =
        "FORGE7 FullscreenPlugin:"
        " fullscreen=" + getBounds().toString()
        + " plugin=" + pluginEditorCanvas.getPluginDisplayNameForDiagnostics()
        + " format=" + pluginEditorCanvas.getPluginFormatNameForDiagnostics()
        + " natural=" + juce::String(pluginEditorCanvas.getNaturalEditorWidth()) + "x"
        + juce::String(pluginEditorCanvas.getNaturalEditorHeight())
        + " pluginViewportBounds=" + currentPluginViewportBounds.toString()
        + " viewportFrame=" + vp.toString()
        + " canvas=" + pluginEditorCanvas.getBounds().toString()
        + " editorInCanvas=" + editorInCanvas.toString()
        + " sizingMode=" + juce::String(static_cast<int>(pluginEditorCanvas.getSizingMode()))
        + " policyAllowsResize=" + (pluginEditorCanvas.isResizableUnderCurrentPolicy() ? "yes" : "no")
        + " viewMode=" + modeStr
        + " pan=(" + juce::String(pluginEditorCanvas.getPanX(), 1) + ","
        + juce::String(pluginEditorCanvas.getPanY(), 1) + ")"
        + " " + pluginEditorCanvas.describeHostedEditorLayoutForDiagnostics();

    Logger::info(diag);
    DBG(diag);

    // Heads-up for the macOS/Windows native cases where a hosted child window may draw outside
    // pluginViewportFrame regardless of JUCE clipping. Layout above guarantees the frame itself
    // never overlaps chrome; the warning helps explain residual visual artifacts to maintainers.
    if (pluginEditorCanvas.hostedEditorMayExceedClipping())
        Logger::info("FORGE7 FullscreenPlugin: Vendor GUI appears to use native drawing that may not obey JUCE clipping.");
}

void FullscreenPluginEditorComponent::refreshPanControlsFromCanvas()
{
    const bool canX = pluginEditorCanvas.canScrollX();
    const bool canY = pluginEditorCanvas.canScrollY();

    // Hide scrollbars when content fits, like a browser. Layout still reserves the band so the
    // workspace size doesn't jitter as scrollbars come and go during pan/resize operations.
    panXSlider.setVisible(canX);
    panYSlider.setVisible(canY);

    panXSlider.setEnabled(canX);
    panYSlider.setEnabled(canY);

    panXSlider.setValue(pluginEditorCanvas.getScrollX01(), juce::dontSendNotification);
    panYSlider.setValue(1.0 - static_cast<double>(pluginEditorCanvas.getScrollY01()), juce::dontSendNotification);
}

void FullscreenPluginEditorComponent::rebuildParameterListModel()
{
    parameterRows.clear();

    if (appContext.parameterMappingManager != nullptr)
        parameterRows = appContext.parameterMappingManager->getAutomatableParametersForSlot(slotIndex);

    parameterList.updateContent();
    parameterList.repaint();
}

void FullscreenPluginEditorComponent::refreshMappingStrip()
{
    if (appContext.parameterMappingManager == nullptr || appContext.sceneManager == nullptr)
        return;

    auto* scene = appContext.sceneManager->getActiveScene();

    if (scene == nullptr)
        return;

    scene->clampActiveVariationIndex();

    const auto& vars = scene->getVariations();
    const int vi = scene->getActiveChainVariationIndex();

    if (!juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) || vars[static_cast<size_t>(vi)] == nullptr)
        return;

    const juce::String sid = scene->getSceneId();
    const juce::String vid = vars[static_cast<size_t>(vi)]->getVariationId();

    const auto rows = appContext.parameterMappingManager->getAllMappings();

    const HardwareControlId knobs[4] = {
        HardwareControlId::Knob1,
        HardwareControlId::Knob2,
        HardwareControlId::Knob3,
        HardwareControlId::Knob4,
    };

    for (int i = 0; i < 4; ++i)
        knobMappingLabels[static_cast<size_t>(i)].setText(juce::String("K") + juce::String(i + 1) + ": "
                                                               + mappingLabelForKnob(rows, sid, vid, slotIndex, knobs[i]),
                                                            juce::dontSendNotification);

    assignMappingLabels[0].setText(juce::String("Button 1: ")
                                       + mappingLabelForKnob(rows, sid, vid, slotIndex, HardwareControlId::AssignButton1),
                                   juce::dontSendNotification);
    assignMappingLabels[1].setText(juce::String("Button 2: ")
                                       + mappingLabelForKnob(rows, sid, vid, slotIndex, HardwareControlId::AssignButton2),
                                   juce::dontSendNotification);

    if (appContext.pluginHostManager != nullptr)
        if (auto* chain = appContext.pluginHostManager->getPluginChain())
        {
            const auto info = chain->getSlotInfo(slotIndex);
            titleLabel.setText("Slot " + juce::String(slotIndex + 1) + " - " + info.slotDisplayName,
                               juce::dontSendNotification);
        }
}

int FullscreenPluginEditorComponent::getNumRows()
{
    return parameterRows.size();
}

void FullscreenPluginEditorComponent::paintListBoxItem(const int rowNumber,
                                                       juce::Graphics& g,
                                                       const int width,
                                                       const int height,
                                                       const bool rowIsSelected)
{
    if (!juce::isPositiveAndBelow(rowNumber, parameterRows.size()))
        return;

    auto r = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(4.0f);

    if (rowIsSelected)
    {
        g.setColour(accent().withAlpha(0.42f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(accent().brighter(0.15f));
        g.drawRoundedRectangle(r, 6.0f, 2.0f);
    }
    else
    {
        g.setColour(panel().brighter(0.06f));
        g.fillRoundedRectangle(r, 6.0f);
    }

    g.setColour(text());
    g.setFont(15.0f);
    g.drawText(parameterRows.getReference(rowNumber).name,
               juce::Rectangle<int>(0, 0, width, height).reduced(10, 0),
               juce::Justification::centredLeft,
               true);
}

void FullscreenPluginEditorComponent::selectedRowsChanged(int)
{
}

void FullscreenPluginEditorComponent::listBoxItemClicked(const int row, const juce::MouseEvent&)
{
    armParameterAssignmentForSelectedRow(row);
}

void FullscreenPluginEditorComponent::armParameterAssignmentForSelectedRow(const int rowIndex)
{
    if (!assignModeToggle.getToggleState())
        return;

    if (!juce::isPositiveAndBelow(rowIndex, parameterRows.size()))
        return;

    if (appContext.parameterMappingManager == nullptr)
        return;

    const auto& p = parameterRows.getReference(rowIndex);

    appContext.parameterMappingManager->prepareKnobAssignmentToNextHardwareMove(slotIndex,
                                                                                p.parameterId,
                                                                                p.parameterIndex,
                                                                                p.name);
}

void FullscreenPluginEditorComponent::syncEncoderFocus()
{
    if (appContext.encoderNavigator == nullptr)
        return;

    std::vector<EncoderFocusItem> items;

    items.push_back({ &backButton, [this]() { backButton.triggerClick(); }, {} });
    items.push_back({ &assignModeToggle, [this]() { assignModeToggle.triggerClick(); }, {} });

    // Plugin workspace: encoder press toggles primary pan axis; rotate pans + syncs scrollbars.
    items.push_back({ &pluginEditorCanvas,
                      [this]()
                      {
                          pluginEditorCanvas.toggleEncoderPanAxis();
                      },
                      [this](const int d)
                      {
                          pluginEditorCanvas.panWithEncoderDetents(d);
                          refreshPanControlsFromCanvas();
                      } });

    // Vertical scrollbar - normalized 0..1; small step matches a comfortable detent feel.
    if (pluginEditorCanvas.canScrollY())
    {
        items.push_back({ &panYSlider,
                          []() {},
                          [this](const int d)
                          {
                              const double step = 0.05 * (d > 0 ? 1.0 : -1.0);
                              panYSlider.setValue(juce::jlimit(0.0, 1.0, panYSlider.getValue() - step),
                                                  juce::sendNotificationSync);
                          } });
    }

    if (pluginEditorCanvas.canScrollX())
    {
        items.push_back({ &panXSlider,
                          []() {},
                          [this](const int d)
                          {
                              const double step = 0.05 * (d > 0 ? 1.0 : -1.0);
                              panXSlider.setValue(juce::jlimit(0.0, 1.0, panXSlider.getValue() + step),
                                                  juce::sendNotificationSync);
                          } });
    }

    if (assignModeToggle.getToggleState())
    {
        items.push_back({ &parameterList,
                          [this]()
                          {
                              const int r = parameterList.getSelectedRow();

                              if (juce::isPositiveAndBelow(r, parameterRows.size()))
                                  armParameterAssignmentForSelectedRow(r);
                          },
                          [this](const int d)
                          {
                              const int n = parameterRows.size();
                              if (n <= 0)
                                  return;
                              int r = parameterList.getSelectedRow();
                              if (r < 0)
                                  r = 0;
                              else
                                  r = juce::jlimit(0, n - 1, r + d);
                              parameterList.selectRow(r);
                              parameterList.scrollToEnsureRowIsOnscreen(r);
                          } });
    }

    items.push_back({ &closeButton, [this]() { closeButton.triggerClick(); }, {} });

    appContext.encoderNavigator->setModalFocusChain(std::move(items));
}

} // namespace forge7
