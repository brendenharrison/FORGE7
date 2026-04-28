#include "FullscreenPluginEditorComponent.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include "../App/AppContext.h"
#include "../Audio/AudioEngine.h"
#include "CpuMeter.h"
#include "../Controls/EncoderFocusTypes.h"
#include "../Controls/EncoderNavigator.h"
#include "../Controls/HardwareControlTypes.h"
#include "../Controls/ParameterMappingManager.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"

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

    return juce::String("—");
}
} // namespace

FullscreenPluginEditorComponent::FullscreenPluginEditorComponent(AppContext& context,
                                                                  const int pluginSlotIndex,
                                                                  std::function<void()> onCloseRequested)
    : appContext(context)
    , slotIndex(pluginSlotIndex)
    , onClose(std::move(onCloseRequested))
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    setOpaque(true);

    // Add first so the hosted VST/editor view stays behind all FORGE chrome (labels, mapping strip).
    addAndMakeVisible(pluginEditorCanvas);

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
        parameterList.setVisible(assignModeToggle.getToggleState());
        assignHintLabel.setVisible(assignModeToggle.getToggleState());
        resized();
        syncEncoderFocus();

        if (appContext.parameterMappingManager != nullptr && !assignModeToggle.getToggleState())
            appContext.parameterMappingManager->cancelKnobAssignmentLearn();
    };
    addAndMakeVisible(assignModeToggle);

    assignHintLabel.setText("Select a parameter, then twist K1–K4 to assign (or use Assign buttons for toggles).",
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

    for (auto& l : assignMappingLabels)
    {
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(13.0f));
        l.setColour(juce::Label::textColourId, text());
        addAndMakeVisible(l);
    }

    viewFitHeight.setColour(juce::TextButton::buttonColourId, panel().brighter(0.08f));
    viewFitHeight.setColour(juce::TextButton::textColourOffId, text());
    viewFitHeight.onClick = [this]()
    {
        pluginEditorCanvas.setViewMode(PluginEditorCanvas::ViewMode::FitHeight);
    };
    addAndMakeVisible(viewFitHeight);

    viewFitWidth.setColour(juce::TextButton::buttonColourId, panel().brighter(0.08f));
    viewFitWidth.setColour(juce::TextButton::textColourOffId, text());
    viewFitWidth.onClick = [this]()
    {
        pluginEditorCanvas.setViewMode(PluginEditorCanvas::ViewMode::FitWidth);
    };
    addAndMakeVisible(viewFitWidth);

    viewFitAll.setColour(juce::TextButton::buttonColourId, panel().brighter(0.08f));
    viewFitAll.setColour(juce::TextButton::textColourOffId, text());
    viewFitAll.onClick = [this]()
    {
        pluginEditorCanvas.setViewMode(PluginEditorCanvas::ViewMode::FitAll);
    };
    addAndMakeVisible(viewFitAll);

    viewActual100.setColour(juce::TextButton::buttonColourId, panel().brighter(0.08f));
    viewActual100.setColour(juce::TextButton::textColourOffId, text());
    viewActual100.onClick = [this]()
    {
        pluginEditorCanvas.setViewMode(PluginEditorCanvas::ViewMode::Actual100);
    };
    addAndMakeVisible(viewActual100);

    juce::AudioPluginInstance* instance = nullptr;

    if (appContext.pluginHostManager != nullptr)
        if (auto* chain = appContext.pluginHostManager->getPluginChain())
            if (auto* slot = chain->getSlot(static_cast<size_t>(slotIndex)))
                instance = slot->getHostedInstance();

    if (instance != nullptr)
    {
        if (instance->hasEditor())
            embeddedEditor.reset(instance->createEditorIfNeeded());

        if (embeddedEditor == nullptr)
            embeddedEditor = std::make_unique<juce::GenericAudioProcessorEditor>(*instance);
    }

    if (embeddedEditor != nullptr)
        pluginEditorCanvas.setHostedEditor(embeddedEditor.get());

    rebuildParameterListModel();

    refreshMappingStrip();
    syncEncoderFocus();
    startTimerHz(8);
}

FullscreenPluginEditorComponent::~FullscreenPluginEditorComponent()
{
    stopTimer();

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

    if (appContext.sceneManager != nullptr)
    {
        const int si = appContext.sceneManager->getActiveSceneIndex();
        const auto& scenes = appContext.sceneManager->getScenes();

        juce::String line = "Scene";

        if (juce::isPositiveAndBelow(si, static_cast<int>(scenes.size())) && scenes[static_cast<size_t>(si)] != nullptr)
        {
            const auto& sc = *scenes[static_cast<size_t>(si)];
            line = "Scene " + juce::String(si + 1) + " · " + sc.getSceneName();

            const auto& vars = sc.getVariations();
            const int vi =
                vars.empty()
                    ? 0
                    : juce::jlimit(0, static_cast<int>(vars.size()) - 1, sc.getActiveChainVariationIndex());

            if (juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) && vars[static_cast<size_t>(vi)] != nullptr)
                line += "   ·   Var: " + vars[static_cast<size_t>(vi)]->getVariationName();
        }

        sceneVarLabel.setText(line, juce::dontSendNotification);
    }
}

void FullscreenPluginEditorComponent::resized()
{
    auto area = getLocalBounds().reduced(10, 8);

    auto top = area.removeFromTop(48);
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

    area.removeFromTop(8);

    {
        auto viewRow = area.removeFromTop(34);
        const int gap = 6;
        const int nBtns = 4;
        const int btnW = juce::jmax(56, (viewRow.getWidth() - gap * (nBtns - 1)) / nBtns);

        viewFitHeight.setBounds(viewRow.removeFromLeft(btnW).reduced(0, 2));
        viewRow.removeFromLeft(gap);
        viewFitWidth.setBounds(viewRow.removeFromLeft(btnW).reduced(0, 2));
        viewRow.removeFromLeft(gap);
        viewFitAll.setBounds(viewRow.removeFromLeft(btnW).reduced(0, 2));
        viewRow.removeFromLeft(gap);
        viewActual100.setBounds(viewRow.removeFromLeft(btnW).reduced(0, 2));
    }

    area.removeFromTop(6);

    auto bottom = area.removeFromBottom(assignModeToggle.getToggleState() ? 220 : 108);

    auto strip = bottom.removeFromBottom(72).reduced(0, 4);
    closeButton.setBounds(strip.removeFromRight(96).reduced(4, 4));
    strip.removeFromRight(6);

    const int sixth = juce::jmax(72, strip.getWidth() / 6);

    for (int i = 0; i < 4; ++i)
        knobMappingLabels[static_cast<size_t>(i)].setBounds(strip.removeFromLeft(sixth).reduced(4, 0));

    for (int i = 0; i < 2; ++i)
        assignMappingLabels[static_cast<size_t>(i)].setBounds(strip.removeFromLeft(sixth).reduced(4, 0));

    if (assignModeToggle.getToggleState())
    {
        assignHintLabel.setBounds(bottom.removeFromTop(22));
        parameterList.setBounds(bottom);
    }

    pluginEditorCanvas.setBounds(area);
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

    assignMappingLabels[0].setText(juce::String("A1: ")
                                       + mappingLabelForKnob(rows, sid, vid, slotIndex, HardwareControlId::AssignButton1),
                                   juce::dontSendNotification);
    assignMappingLabels[1].setText(juce::String("A2: ")
                                       + mappingLabelForKnob(rows, sid, vid, slotIndex, HardwareControlId::AssignButton2),
                                   juce::dontSendNotification);

    if (appContext.pluginHostManager != nullptr)
        if (auto* chain = appContext.pluginHostManager->getPluginChain())
        {
            const auto info = chain->getSlotInfo(slotIndex);
            titleLabel.setText("Slot " + juce::String(slotIndex + 1) + " · " + info.slotDisplayName,
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

    items.push_back({ &viewFitHeight, [this]() { viewFitHeight.triggerClick(); }, {} });
    items.push_back({ &viewFitWidth, [this]() { viewFitWidth.triggerClick(); }, {} });
    items.push_back({ &viewFitAll, [this]() { viewFitAll.triggerClick(); }, {} });
    items.push_back({ &viewActual100, [this]() { viewActual100.triggerClick(); }, {} });

    /** Encoder rotate pans the plugin canvas when zoomed; encoder press toggles horizontal vs vertical pan axis. */
    items.push_back({ &pluginEditorCanvas,
                      [this]()
                      {
                          pluginEditorCanvas.toggleEncoderPanAxis();
                      },
                      [this](const int d)
                      {
                          pluginEditorCanvas.panWithEncoderDetents(d);
                      } });

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
