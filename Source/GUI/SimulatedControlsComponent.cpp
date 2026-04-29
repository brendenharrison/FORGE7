#include "SimulatedControlsComponent.h"

#include <cmath>

#include "../App/AppConfig.h"
#include "../App/AppContext.h"
#include "../App/MainComponent.h"
#include "../Controls/ControlManager.h"
#include "../Controls/HardwareControlTypes.h"
#include "../Controls/ParameterMappingDescriptor.h"
#include "../Controls/ParameterMappingManager.h"
#include "../GUI/RackViewComponent.h"
#include "../PluginHost/PluginChain.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"
#include "ProjectLibraryDialogs.h"
#include "../Storage/ProjectSerializer.h"
#include "../Utilities/Logger.h"
#include "NavigationStatus.h"

namespace forge7
{
namespace
{
juce::Colour panelBg() noexcept { return juce::Colour(0xff1a1d24); }
juce::Colour text() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour muted() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour accent() noexcept { return juce::Colour(0xff6bc4ff); }

HardwareControlId knobIdForIndex(const int i) noexcept
{
    switch (i)
    {
        case 0:
            return HardwareControlId::Knob1;
        case 1:
            return HardwareControlId::Knob2;
        case 2:
            return HardwareControlId::Knob3;
        default:
            return HardwareControlId::Knob4;
    }
}

const ParameterMappingDescriptor* findKnobMappingFor(const juce::Array<ParameterMappingDescriptor>& rows,
                                                     const juce::String& sceneId,
                                                     const juce::String& variationId,
                                                     const HardwareControlId hid)
{
    for (const auto& row : rows)
    {
        if (row.hardwareControlId != hid)
            continue;

        if (row.sceneId == sceneId && row.chainVariationId == variationId)
            return &row;
    }

    return nullptr;
}

} // namespace

SimulatedControlsComponent::SimulatedControlsComponent(AppContext& context)
    : appContext(context)
{
    setOpaque(true);

    assign1Bridge.owner = this;
    assign1Bridge.assignIndex = 1;
    assign2Bridge.owner = this;
    assign2Bridge.assignIndex = 2;

    for (int i = 0; i < 4; ++i)
    {
        auto& s = knobs[static_cast<size_t>(i)];
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRange(0.0, 1.0, 0.001);
        s.setValue(0.5);
        s.addListener(this);
        addAndMakeVisible(s);

        knobRelLabels[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        knobRelLabels[static_cast<size_t>(i)].setFont(juce::Font(12.0f));
        knobRelLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId, text());
        knobRelLabels[static_cast<size_t>(i)].setText("K" + juce::String(i + 1) + " rel",
                                                      juce::dontSendNotification);
        addAndMakeVisible(knobRelLabels[static_cast<size_t>(i)]);

        knobValueLabels[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        knobValueLabels[static_cast<size_t>(i)].setFont(juce::Font(11.0f));
        knobValueLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId, muted());
        knobValueLabels[static_cast<size_t>(i)].setText("--", juce::dontSendNotification);
        addAndMakeVisible(knobValueLabels[static_cast<size_t>(i)]);

        knobDownButtons[static_cast<size_t>(i)].setButtonText("-");
        knobUpButtons[static_cast<size_t>(i)].setButtonText("+");

        knobDownButtons[static_cast<size_t>(i)].setColour(juce::TextButton::buttonColourId, panelBg().brighter(0.12f));
        knobDownButtons[static_cast<size_t>(i)].setColour(juce::TextButton::textColourOffId, text());
        knobUpButtons[static_cast<size_t>(i)].setColour(juce::TextButton::buttonColourId, panelBg().brighter(0.12f));
        knobUpButtons[static_cast<size_t>(i)].setColour(juce::TextButton::textColourOffId, text());

        const int idx = i;
        knobDownButtons[static_cast<size_t>(idx)].onClick = [this, idx]()
        {
            emitKnobRelativeDelta(knobIdForIndex(idx), -0.01f);
        };

        knobUpButtons[static_cast<size_t>(idx)].onClick = [this, idx]()
        {
            emitKnobRelativeDelta(knobIdForIndex(idx), 0.01f);
        };

        addAndMakeVisible(knobDownButtons[static_cast<size_t>(i)]);
        addAndMakeVisible(knobUpButtons[static_cast<size_t>(i)]);
    }

    refreshAssignableKnobDisplaysFromPluginState();

    shortcutsHeading.setJustificationType(juce::Justification::centredLeft);
    shortcutsHeading.setFont(juce::Font(13.0f));
    shortcutsHeading.setColour(juce::Label::textColourId, accent());
    addAndMakeVisible(shortcutsHeading);

    debugHeading.setJustificationType(juce::Justification::centredLeft);
    debugHeading.setFont(juce::Font(13.0f));
    debugHeading.setColour(juce::Label::textColourId, accent());
    addAndMakeVisible(debugHeading);

    for (auto* l : { &sceneLabel,
                     &variationLabel,
                     &lastEventLabel,
                     &knobSummaryLabel,
                     &encoderFocusLabel,
                     &slotLabel,
                     &uiSurfaceLabel })
    {
        l->setJustificationType(juce::Justification::topLeft);
        l->setFont(juce::Font(12.0f));
        l->setColour(juce::Label::textColourId, text());
        addAndMakeVisible(*l);
    }

    encoderSectionLabel.setJustificationType(juce::Justification::centredLeft);
    encoderSectionLabel.setFont(juce::Font(13.0f));
    encoderSectionLabel.setColour(juce::Label::textColourId, accent());
    addAndMakeVisible(encoderSectionLabel);

    wireButtons();

    assign1Button.addMouseListener(&assign1Bridge, false);
    assign2Button.addMouseListener(&assign2Bridge, false);

    refreshDebugLabels();
    startTimerHz(8);
}

SimulatedControlsComponent::~SimulatedControlsComponent()
{
    stopTimer();

    assign1Button.removeMouseListener(&assign1Bridge);
    assign2Button.removeMouseListener(&assign2Bridge);

    for (auto& k : knobs)
        k.removeListener(this);
}

void SimulatedControlsComponent::paint(juce::Graphics& g)
{
    g.fillAll(panelBg());
}

void SimulatedControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(10);

    auto dbg = area.removeFromBottom(168);
    debugHeading.setBounds(dbg.removeFromTop(18));
    dbg.removeFromTop(4);

    const int dbgRow = 22;
    sceneLabel.setBounds(dbg.removeFromTop(dbgRow));
    variationLabel.setBounds(dbg.removeFromTop(dbgRow));
    lastEventLabel.setBounds(dbg.removeFromTop(dbgRow));
    knobSummaryLabel.setBounds(dbg.removeFromTop(dbgRow));
    encoderFocusLabel.setBounds(dbg.removeFromTop(dbgRow));
    slotLabel.setBounds(dbg.removeFromTop(dbgRow));
    uiSurfaceLabel.setBounds(dbg.removeFromTop(dbgRow));

    area.removeFromBottom(8);

    auto shortcuts = area.removeFromBottom(168);
    shortcutsHeading.setBounds(shortcuts.removeFromTop(18));
    shortcuts.removeFromTop(4);

    auto row1 = shortcuts.removeFromTop(34);
    shortcutEditMode.setBounds(row1.removeFromLeft(juce::jmax(120, row1.getWidth() / 3)).reduced(2, 2));
    shortcutPerfMode.setBounds(row1.removeFromLeft(juce::jmax(120, row1.getWidth() / 2)).reduced(2, 2));
    shortcutPluginBrowser.setBounds(row1.reduced(2, 2));

    auto row2 = shortcuts.removeFromTop(34);
    shortcutInspector.setBounds(row2.removeFromLeft(juce::jmax(120, row2.getWidth() / 3)).reduced(2, 2));
    shortcutSave.setBounds(row2.removeFromLeft(juce::jmax(120, row2.getWidth() / 2)).reduced(2, 2));
    shortcutLoad.setBounds(row2.reduced(2, 2));

    auto row3 = shortcuts.removeFromTop(34);
    shortcutExportProject.setBounds(row3.removeFromLeft(juce::jmax(120, row3.getWidth() / 2)).reduced(2, 2));
    shortcutImportProject.setBounds(row3.reduced(2, 2));

    shortcuts.removeFromTop(6);
    shortcutLibraryStatusLabel.setBounds(shortcuts);

    area.removeFromBottom(8);

    encoderSectionLabel.setBounds(area.removeFromTop(18));

    auto encRow = area.removeFromTop(36);
    encoderLeftButton.setBounds(encRow.removeFromLeft(encRow.getWidth() / 4).reduced(2, 2));
    encoderRightButton.setBounds(encRow.removeFromLeft(encRow.getWidth() / 3).reduced(2, 2));
    encoderPressButton.setBounds(encRow.removeFromLeft(encRow.getWidth() / 2).reduced(2, 2));
    encoderLongPressButton.setBounds(encRow.reduced(2, 2));

    area.removeFromBottom(8);

    auto chainRow = area.removeFromTop(36);
    chainPrevButton.setBounds(chainRow.removeFromLeft(chainRow.getWidth() / 2).reduced(2, 2));
    chainNextButton.setBounds(chainRow.reduced(2, 2));

    area.removeFromBottom(8);

    auto assignRow = area.removeFromTop(36);
    assign1Button.setBounds(assignRow.removeFromLeft(assignRow.getWidth() / 2).reduced(2, 2));
    assign2Button.setBounds(assignRow.reduced(2, 2));

    area.removeFromBottom(8);

    auto knobsRow = area.removeFromTop(164);
    const int kw = juce::jmax(64, knobsRow.getWidth() / 4);

    for (int i = 0; i < 4; ++i)
    {
        auto col = knobsRow.removeFromLeft(kw).reduced(2, 0);
        knobsRow.removeFromLeft(4);

        knobRelLabels[static_cast<size_t>(i)].setBounds(col.removeFromTop(18));
        col.removeFromTop(2);
        knobValueLabels[static_cast<size_t>(i)].setBounds(col.removeFromTop(22));

        auto btnRow = col.removeFromTop(28);
        knobDownButtons[static_cast<size_t>(i)].setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2).reduced(2, 2));
        knobUpButtons[static_cast<size_t>(i)].setBounds(btnRow.reduced(2, 2));

        col.removeFromTop(4);

        if (appContext.appConfig != nullptr && appContext.appConfig->getSimDevAbsoluteKnobTest())
            knobs[static_cast<size_t>(i)].setBounds(col.removeFromTop(88));
        else
            knobs[static_cast<size_t>(i)].setBounds({});
    }

    refreshAssignableKnobDisplaysFromPluginState();
}

void SimulatedControlsComponent::sliderValueChanged(juce::Slider* slider)
{
    if (appContext.controlManager == nullptr)
        return;

    if (appContext.appConfig == nullptr || ! appContext.appConfig->getSimDevAbsoluteKnobTest())
        return;

    for (int i = 0; i < 4; ++i)
    {
        if (slider == &knobs[static_cast<size_t>(i)])
        {
            emitKnobAbsoluteForDevTools(knobIdForIndex(i), static_cast<float>(slider->getValue()));
            knobValueLabels[static_cast<size_t>(i)].setText(juce::String(slider->getValue(), 3),
                                                              juce::dontSendNotification);
            return;
        }
    }
}

void SimulatedControlsComponent::emitKnobRelativeDelta(const HardwareControlId id,
                                                       const float pluginNormalizedDelta)
{
    if (appContext.controlManager == nullptr)
        return;

    if (std::abs(pluginNormalizedDelta) <= 0.0f)
        return;

    HardwareControlEvent e {};
    e.id = id;
    e.type = HardwareControlType::RelativeDelta;
    e.source = HardwareControlSource::SimulatedGui;
    e.value = juce::jlimit(-0.5f, 0.5f, pluginNormalizedDelta);

    lastEmittedId = id;
    lastEmittedValue = e.value;
    lastEmittedExtra = "rel";

    appContext.controlManager->submitHardwareEvent(e);
}

void SimulatedControlsComponent::refreshAssignableKnobDisplaysFromPluginState()
{
    const bool dev = appContext.appConfig != nullptr && appContext.appConfig->getSimDevAbsoluteKnobTest();

    for (int i = 0; i < 4; ++i)
        knobs[static_cast<size_t>(i)].setVisible(dev);

    if (appContext.parameterMappingManager == nullptr || appContext.sceneManager == nullptr)
        return;

    const NavigationStatus nav = computeNavigationStatus(appContext);
    const juce::Array<ParameterMappingDescriptor> rows = appContext.parameterMappingManager->getAllMappings();

    for (int k = 0; k < 4; ++k)
    {
        const auto hid = knobIdForIndex(k);
        const auto* row = findKnobMappingFor(rows, nav.sceneId, nav.chainId, hid);
        juce::String line = "--";

        if (row != nullptr)
        {
            float p01 = 0.0f;

            if (appContext.parameterMappingManager->tryReadMappedParameterNormalized(*row, p01))
            {
                const float arc = ParameterMappingManager::hardwareArc01ForHud(*row, p01);
                const juce::String vt =
                    appContext.parameterMappingManager->getMappedParameterValueText(*row);

                line = vt.isNotEmpty() ? vt : (juce::String(juce::roundToInt(arc * 100.0f)) + "%");

                if (dev)
                    knobs[static_cast<size_t>(k)].setValue(static_cast<double>(arc),
                                                           juce::dontSendNotification);
            }
        }

        knobValueLabels[static_cast<size_t>(k)].setText(line, juce::dontSendNotification);
    }
}

void SimulatedControlsComponent::emitKnobAbsoluteForDevTools(const HardwareControlId id, const float normalized01)
{
    if (appContext.controlManager == nullptr)
        return;

    HardwareControlEvent e {};
    e.id = id;
    e.type = HardwareControlType::AbsoluteNormalized;
    e.source = HardwareControlSource::SimulatedGui;
    e.value = juce::jlimit(0.0f, 1.0f, normalized01);

    lastEmittedId = id;
    lastEmittedValue = e.value;
    lastEmittedExtra = {};

    // Dev-only: tracing for simulated control input.
    static float lastLogged[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    const int kidx = knobIndexFromId(id);
    if (kidx >= 0 && kidx < 4)
    {
        const float prev = lastLogged[kidx];
        if (prev < 0.0f || std::abs(prev - e.value) >= 0.02f)
        {
            lastLogged[kidx] = e.value;
            Logger::info("FORGE7 SimHW: K" + juce::String(kidx + 1) + " moved value=" + juce::String(e.value, 3));
        }
    }

    appContext.controlManager->submitHardwareEvent(e);
}

void SimulatedControlsComponent::emitEncoderDelta(const int delta)
{
    if (appContext.controlManager == nullptr || delta == 0)
        return;

    HardwareControlEvent e {};
    e.id = HardwareControlId::EncoderRotate;
    e.type = HardwareControlType::RelativeDelta;
    e.source = HardwareControlSource::SimulatedGui;
    e.value = static_cast<float>(delta);

    lastEmittedId = e.id;
    lastEmittedValue = e.value;
    lastEmittedExtra = "delta";

    appContext.controlManager->submitHardwareEvent(e);
}

void SimulatedControlsComponent::emitEncoderPress(const HardwareControlId pressOrLong)
{
    if (appContext.controlManager == nullptr)
        return;

    HardwareControlEvent e {};
    e.id = pressOrLong;
    e.type = HardwareControlType::ButtonPressed;
    e.source = HardwareControlSource::SimulatedGui;
    e.value = 1.0f;

    lastEmittedId = e.id;
    lastEmittedValue = e.value;
    lastEmittedExtra = {};

    if (pressOrLong == HardwareControlId::EncoderLongPress)
        Logger::info("FORGE7 SimHW: EncoderLongPress emitted");

    appContext.controlManager->submitHardwareEvent(e);
}

void SimulatedControlsComponent::wireButtons()
{
    auto style = [](juce::Button& b)
    {
        b.setColour(juce::TextButton::buttonColourId, panelBg().brighter(0.12f));
        b.setColour(juce::TextButton::textColourOffId, text());
    };

    shortcutLibraryStatusLabel.setJustificationType(juce::Justification::topLeft);
    shortcutLibraryStatusLabel.setFont(juce::Font(11.0f));
    shortcutLibraryStatusLabel.setColour(juce::Label::textColourId, muted());
    shortcutLibraryStatusLabel.setText("Projects save to ~/Documents/FORGE7/Projects (name only).",
                                       juce::dontSendNotification);
    addAndMakeVisible(shortcutLibraryStatusLabel);

    for (auto* b : { &assign1Button,
                     &assign2Button,
                     &chainPrevButton,
                     &chainNextButton,
                     &encoderLeftButton,
                     &encoderRightButton,
                     &encoderPressButton,
                     &encoderLongPressButton,
                     &shortcutEditMode,
                     &shortcutPerfMode,
                     &shortcutPluginBrowser,
                     &shortcutInspector,
                     &shortcutSave,
                     &shortcutLoad,
                     &shortcutExportProject,
                     &shortcutImportProject })
    {
        style(*b);
        addAndMakeVisible(*b);
    }

    chainPrevButton.onClick = [this]()
    {
        if (appContext.controlManager == nullptr)
            return;

        HardwareControlEvent e {};
        e.id = HardwareControlId::ChainPreviousButton;
        e.type = HardwareControlType::ButtonPressed;
        e.source = HardwareControlSource::SimulatedGui;
        e.value = 1.0f;
        lastEmittedId = e.id;
        appContext.controlManager->submitHardwareEvent(e);
    };

    chainNextButton.onClick = [this]()
    {
        if (appContext.controlManager == nullptr)
            return;

        HardwareControlEvent e {};
        e.id = HardwareControlId::ChainNextButton;
        e.type = HardwareControlType::ButtonPressed;
        e.source = HardwareControlSource::SimulatedGui;
        e.value = 1.0f;
        lastEmittedId = e.id;
        appContext.controlManager->submitHardwareEvent(e);
    };

    encoderLeftButton.onClick = [this]() { emitEncoderDelta(-1); };
    encoderRightButton.onClick = [this]() { emitEncoderDelta(1); };
    encoderPressButton.onClick = [this]() { emitEncoderPress(HardwareControlId::EncoderPress); };
    encoderLongPressButton.onClick = [this]() { emitEncoderPress(HardwareControlId::EncoderLongPress); };

    shortcutEditMode.onClick = [this]()
    {
        if (appContext.mainComponent != nullptr)
            appContext.mainComponent->setEditMode(true);
    };

    shortcutPerfMode.onClick = [this]()
    {
        if (appContext.mainComponent != nullptr)
            appContext.mainComponent->setEditMode(false);
    };

    shortcutPluginBrowser.onClick = [this]()
    {
        if (appContext.mainComponent == nullptr)
            return;

        appContext.mainComponent->setEditMode(true);
        appContext.mainComponent->openPluginBrowserFromDevTools();
    };

    shortcutInspector.onClick = [this]()
    {
        if (appContext.mainComponent == nullptr)
            return;

        appContext.mainComponent->setEditMode(true);
        appContext.mainComponent->focusPluginInspectorFromDevTools();
    };

    shortcutSave.onClick = [this]()
    {
        runSaveProjectToLibraryDialog(this,
                                      appContext,
                                      [this](const juce::String& status)
                                      {
                                          shortcutLibraryStatusLabel.setText(status, juce::dontSendNotification);
                                      });
    };

    shortcutLoad.onClick = [this]()
    {
        runLoadProjectFromLibraryBrowser(this,
                                         appContext,
                                         [this](const juce::String& status)
                                         {
                                             shortcutLibraryStatusLabel.setText(status, juce::dontSendNotification);
                                         });
    };

    shortcutExportProject.onClick = [this]()
    {
        runExportProjectWithFileChooser(this, appContext);
    };

    shortcutImportProject.onClick = [this]()
    {
        runImportProjectWithFileChooser(this, appContext);
    };
}

void SimulatedControlsComponent::emitAssignPressed(const int assignIndex)
{
    if (appContext.controlManager == nullptr)
        return;

    HardwareControlEvent e {};
    e.id = assignIndex <= 1 ? HardwareControlId::AssignButton1 : HardwareControlId::AssignButton2;
    e.type = HardwareControlType::ButtonPressed;
    e.source = HardwareControlSource::SimulatedGui;
    e.value = 1.0f;

    lastEmittedId = e.id;
    lastEmittedValue = e.value;

    appContext.controlManager->submitHardwareEvent(e);
}

void SimulatedControlsComponent::emitAssignReleased(const int assignIndex)
{
    if (appContext.controlManager == nullptr)
        return;

    HardwareControlEvent e {};
    e.id = assignIndex <= 1 ? HardwareControlId::AssignButton1 : HardwareControlId::AssignButton2;
    e.type = HardwareControlType::ButtonReleased;
    e.source = HardwareControlSource::SimulatedGui;
    e.value = 0.0f;

    lastEmittedId = e.id;
    lastEmittedValue = e.value;

    appContext.controlManager->submitHardwareEvent(e);
}

void SimulatedControlsComponent::AssignMouseBridge::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (owner != nullptr)
        owner->emitAssignPressed(assignIndex);
}

void SimulatedControlsComponent::AssignMouseBridge::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (owner != nullptr)
        owner->emitAssignReleased(assignIndex);
}

void SimulatedControlsComponent::timerCallback()
{
    refreshDebugLabels();
}

void SimulatedControlsComponent::refreshDebugLabels()
{
    refreshAssignableKnobDisplaysFromPluginState();

    juce::String sceneLine = "Scene: -";
    juce::String chainLine = "Chain: -";

    if (appContext.sceneManager != nullptr)
    {
        const int si = appContext.sceneManager->getActiveSceneIndex();
        const auto& scenes = appContext.sceneManager->getScenes();

        if (juce::isPositiveAndBelow(si, static_cast<int>(scenes.size())) && scenes[static_cast<size_t>(si)] != nullptr)
        {
            const auto& sc = *scenes[static_cast<size_t>(si)];
            sceneLine = "Scene: " + juce::String(si + 1) + " - " + sc.getSceneName();

            const auto& vars = sc.getVariations();
            const int vi =
                vars.empty() ? 0
                             : juce::jlimit(0, static_cast<int>(vars.size()) - 1, sc.getActiveChainVariationIndex());

            if (juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) && vars[static_cast<size_t>(vi)] != nullptr)
            {
                const juce::String name = vars[static_cast<size_t>(vi)]->getVariationName();
                const int oneBased = vi + 1;
                const juce::String idx = oneBased < 10 ? juce::String("0") + juce::String(oneBased)
                                                       : juce::String(oneBased);
                chainLine = "Chain: " + idx
                            + (name.isNotEmpty() ? juce::String(" - ") + name : juce::String());
            }
        }
    }

    sceneLabel.setText(sceneLine, juce::dontSendNotification);
    variationLabel.setText(chainLine, juce::dontSendNotification);

    juce::String ev = "Last event: ";

    switch (lastEmittedId)
    {
        case HardwareControlId::Knob1:
            ev += "K1";
            break;
        case HardwareControlId::Knob2:
            ev += "K2";
            break;
        case HardwareControlId::Knob3:
            ev += "K3";
            break;
        case HardwareControlId::Knob4:
            ev += "K4";
            break;
        case HardwareControlId::AssignButton1:
            ev += "Assign1";
            break;
        case HardwareControlId::AssignButton2:
            ev += "Assign2";
            break;
        case HardwareControlId::ChainPreviousButton:
            ev += "Chain-";
            break;
        case HardwareControlId::ChainNextButton:
            ev += "Chain+";
            break;
        case HardwareControlId::EncoderRotate:
            ev += "EncRot";
            break;
        case HardwareControlId::EncoderPress:
            ev += "EncPress";
            break;
        case HardwareControlId::EncoderLongPress:
            ev += "EncLong";
            break;
    }

    ev += "  value=" + juce::String(lastEmittedValue, 4);

    if (lastEmittedExtra.isNotEmpty())
        ev += "  (" + lastEmittedExtra + ")";

    lastEventLabel.setText(ev, juce::dontSendNotification);

    juce::String knobsSummary = "K1-K4 disp: ";

    for (int i = 0; i < 4; ++i)
        knobsSummary += knobValueLabels[static_cast<size_t>(i)].getText() + (i < 3 ? " | " : " ");

    knobSummaryLabel.setText(knobsSummary, juce::dontSendNotification);

    if (appContext.encoderNavigator != nullptr)
        encoderFocusLabel.setText("Encoder focus: " + appContext.encoderNavigator->getFocusDebugSummary(),
                                  juce::dontSendNotification);
    else
        encoderFocusLabel.setText("Encoder focus: -", juce::dontSendNotification);

    juce::String slotLine = "Selected rack slot: -";

    if (appContext.mainComponent != nullptr)
        if (auto* rack = appContext.mainComponent->getRackView())
            slotLine = "Selected rack slot: "
                       + (rack->getSelectedSlotIndex() < 0
                              ? juce::String("(none)")
                              : juce::String(rack->getSelectedSlotIndex() + 1));

    slotLabel.setText(slotLine, juce::dontSendNotification);

    if (appContext.mainComponent != nullptr)
        uiSurfaceLabel.setText("UI: " + appContext.mainComponent->describeUiSurfaceForDevTools(),
                               juce::dontSendNotification);
    else
        uiSurfaceLabel.setText("UI: -", juce::dontSendNotification);
}

} // namespace forge7
