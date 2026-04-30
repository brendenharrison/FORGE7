#include "PerformanceViewComponent.h"

#include <vector>

#include "../App/AppContext.h"
#include "../App/MainComponent.h"

#include "../Controls/EncoderFocusTypes.h"
#include "../Controls/EncoderNavigator.h"

#include "../Audio/AudioEngine.h"
#include "../PluginHost/PluginHostManager.h"
#include "../Controls/ControlManager.h"
#include "../Controls/HardwareControlTypes.h"
#include "../Controls/ParameterMappingDescriptor.h"
#include "../Controls/ParameterMappingManager.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginSlot.h"
#include "../Scene/ChainVariation.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"
#include "../Utilities/Logger.h"
#include "../App/ProjectSession.h"
#include "HardwareAssignableUi.h"
#include "NavigationStatus.h"
#include "NameEntryModal.h"
#include "UnsavedChangesModal.h"

namespace forge7
{
namespace
{

struct LedDot final : public juce::Component
{
    juce::Colour colour;

    explicit LedDot(juce::Colour c)
        : colour(c)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto e = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(colour.withAlpha(0.35f));
        g.fillEllipse(e.expanded(1.8f));

        g.setColour(colour);
        g.fillEllipse(e);

        g.setColour(colour.brighter(0.25f));
        g.drawEllipse(e, 1.15f);
    }
};

juce::Colour perfBg() noexcept { return juce::Colour(0xff0d0f12); }
juce::Colour perfPanel() noexcept { return juce::Colour(0xff161a20); }
juce::Colour perfText() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour perfMuted() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour perfAccent() noexcept { return juce::Colour(0xff4a9eff); }
juce::Colour perfHighlight() noexcept { return juce::Colour(0xff6bc4ff); }

void styleToolbarButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, perfPanel().brighter(0.08f));
    b.setColour(juce::TextButton::textColourOffId, perfText());
    b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleHudLabel(juce::Label& lab, float fontHeight, juce::Colour c)
{
    lab.setJustificationType(juce::Justification::centredLeft);
    lab.setFont(juce::Font(fontHeight));
    lab.setColour(juce::Label::textColourId, c);
    lab.setMinimumHorizontalScale(0.75f);
}

const ParameterMappingDescriptor* findMappingFor(const juce::Array<ParameterMappingDescriptor>& rows,
                                                 const juce::String& sceneId,
                                                 const juce::String& variationId,
                                                 HardwareControlId hid)
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

//==============================================================================
class PerformanceViewComponent::KnobCard final : public juce::Component
{
public:
    void setParameterTitle(const juce::String& s)
    {
        title = s;
        repaint();
    }

    void setValueText(const juce::String& s)
    {
        valueText = s;
        repaint();
    }

    void setNormalized(float x) noexcept
    {
        normalized = juce::jlimit(0.0f, 1.0f, x);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);

        g.setColour(perfPanel());
        g.fillRoundedRectangle(bounds, 10.0f);

        g.setColour(perfAccent().withAlpha(0.35f));
        g.drawRoundedRectangle(bounds, 10.0f, 1.5f);

        g.setColour(perfMuted());
        g.setFont(juce::Font(13.0f));
        g.drawFittedText(title.isNotEmpty() ? title : juce::String("-"),
                         juce::Rectangle<int>(getLocalBounds().reduced(10, 8).removeFromTop(20)),
                         juce::Justification::centred,
                         2);

        const auto dialArea = getLocalBounds().reduced(14, 36).removeFromBottom(getHeight() / 2 + 10);
        const float sz = static_cast<float>(juce::jmin(dialArea.getWidth(), dialArea.getHeight()));
        auto dial = dialArea.withSizeKeepingCentre(static_cast<int>(sz), static_cast<int>(sz)).toFloat();

        g.setColour(perfPanel().brighter(0.12f));
        g.fillEllipse(dial);

        g.setColour(perfMuted().withAlpha(0.5f));
        g.drawEllipse(dial, 1.2f);

        const float angle0 = juce::MathConstants<float>::pi * 0.75f;
        const float angle1 = juce::MathConstants<float>::pi * 2.25f;
        const float t = normalized;
        const float a = angle0 + (angle1 - angle0) * t;

        const juce::Point<float> centre = dial.getCentre();
        const float r = dial.getWidth() * 0.42f;

        juce::Path arc;
        arc.addCentredArc(centre.x,
                          centre.y,
                          r,
                          r,
                          0.0f,
                          angle0,
                          a,
                          true);
        g.setColour(perfAccent());
        g.strokePath(arc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const juce::Point<float> tip(centre.x + std::cos(a - juce::MathConstants<float>::halfPi) * r * 0.85f,
                                     centre.y + std::sin(a - juce::MathConstants<float>::halfPi) * r * 0.85f);

        g.setColour(perfHighlight());
        g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(tip));

        g.setColour(perfText());
        g.setFont(juce::Font(17.0f));
        g.drawFittedText(valueText.isNotEmpty() ? valueText : juce::String("-"),
                           getLocalBounds().removeFromBottom(28).reduced(8, 0),
                           juce::Justification::centred,
                           1);
    }

private:
    juce::String title;
    juce::String valueText { "-" };
    float normalized { 0.0f };
};

//==============================================================================
PerformanceViewComponent::PerformanceViewComponent(AppContext& context)
    : appContext(context),
      cpuMeter(appContext.audioEngine),
      audioHealthMonitor(appContext.audioEngine)
{
    styleToolbarButton(rackEditButton);
    styleToolbarButton(scenePrevButton);
    styleToolbarButton(sceneNextButton);
    styleToolbarButton(chainPrevButton);
    styleToolbarButton(chainNextButton);
    styleToolbarButton(settingsButton);

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    styleToolbarButton(simHwButton);
    simHwButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->toggleSimulatedControlsPanel();
    };
    addAndMakeVisible(simHwButton);

    simHwHintLabel.setJustificationType(juce::Justification::centredLeft);
    simHwHintLabel.setFont(juce::Font(12.0f));
    simHwHintLabel.setColour(juce::Label::textColourId, perfMuted());
    simHwHintLabel.setText(
        "Sim HW: use in-app drawer for K1-K4 / Button 1-2 / chain / encoder", juce::dontSendNotification);
    addAndMakeVisible(simHwHintLabel);
#endif

    rackEditButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->setEditMode(true);
    };

    scenePrevButton.onClick = [this]()
    {
        if (appContext.projectSession == nullptr)
            return;

        const int before = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveSceneIndex() : -1;
        appContext.projectSession->previousScene();
        const int after = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveSceneIndex() : -1;

        if (appContext.sceneManager != nullptr && appContext.sceneManager->getScenes().size() > 1u)
            if (before == 0 && after != 0)
                Logger::info("FORGE7: Scene - wrapped from first to last");
    };

    sceneNextButton.onClick = [this]()
    {
        if (appContext.projectSession == nullptr)
            return;

        const int before = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveSceneIndex() : -1;
        appContext.projectSession->nextScene();
        const int after = appContext.sceneManager != nullptr ? appContext.sceneManager->getActiveSceneIndex() : -1;

        if (appContext.sceneManager != nullptr && appContext.sceneManager->getScenes().size() > 1u)
            if (after == 0 && before != 0)
                Logger::info("FORGE7: Scene + wrapped from last to first");
    };

    chainPrevButton.onClick = [this]()
    {
        if (appContext.mainComponent != nullptr)
            appContext.mainComponent->handleChainPreviousFromUi();
    };

    chainNextButton.onClick = [this]()
    {
        if (appContext.mainComponent != nullptr)
            appContext.mainComponent->handleChainNextFromUi();
    };

    settingsButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->openSettings();
    };

    addAndMakeVisible(rackEditButton);
    addAndMakeVisible(scenePrevButton);
    addAndMakeVisible(sceneNextButton);
    sceneCountLabel.setJustificationType(juce::Justification::centred);
    sceneCountLabel.setFont(juce::Font(15.0f));
    sceneCountLabel.setColour(juce::Label::textColourId, perfMuted());
    addAndMakeVisible(sceneCountLabel);
    addAndMakeVisible(chainPrevButton);
    addAndMakeVisible(chainNextButton);
    addAndMakeVisible(settingsButton);

    styleHudLabel(bpmStatusLabel, 16.0f, perfText());
    addAndMakeVisible(bpmStatusLabel);

    addAndMakeVisible(cpuMeter);

    projectNameLabel.setJustificationType(juce::Justification::centredLeft);
    projectNameLabel.setFont(juce::Font(13.0f));
    projectNameLabel.setColour(juce::Label::textColourId, perfMuted());
    projectNameLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(projectNameLabel);

    projectDirtyLabel.setJustificationType(juce::Justification::centredLeft);
    projectDirtyLabel.setFont(juce::Font(12.0f));
    projectDirtyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb74d));
    projectDirtyLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(projectDirtyLabel);

    heroSceneLabel.setJustificationType(juce::Justification::centredLeft);
    heroSceneLabel.setFont(juce::Font(34.0f));
    heroSceneLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(heroSceneLabel);

    chainHeaderLabel.setJustificationType(juce::Justification::centredLeft);
    chainHeaderLabel.setFont(juce::Font(22.0f));
    chainHeaderLabel.setColour(juce::Label::textColourId, perfAccent());
    addAndMakeVisible(chainHeaderLabel);

    chainCountLabel.setJustificationType(juce::Justification::centred);
    chainCountLabel.setFont(juce::Font(16.0f));
    chainCountLabel.setColour(juce::Label::textColourId, perfMuted());
    addAndMakeVisible(chainCountLabel);

    for (size_t i = 0; i < knobCards.size(); ++i)
    {
        knobCards[i] = std::make_unique<KnobCard>();
        knobCards[i]->setParameterTitle("K" + juce::String(static_cast<int>(i) + 1));
        addAndMakeVisible(*knobCards[i]);
    }

    assignButton1Led = std::unique_ptr<juce::Component>(new LedDot(button1Colour()));
    addAndMakeVisible(*assignButton1Led);

    assign1TitleLabel.setText("Button 1", juce::dontSendNotification);
    styleHudLabel(assign1TitleLabel, 13.0f, button1Colour());
    addAndMakeVisible(assign1TitleLabel);

    assign1FunctionLabel.setJustificationType(juce::Justification::centredLeft);
    assign1FunctionLabel.setFont(juce::Font(17.0f));
    assign1FunctionLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(assign1FunctionLabel);

    assignButton2Led = std::unique_ptr<juce::Component>(new LedDot(button2Colour()));
    addAndMakeVisible(*assignButton2Led);

    assign2TitleLabel.setText("Button 2", juce::dontSendNotification);
    styleHudLabel(assign2TitleLabel, 13.0f, button2Colour());
    addAndMakeVisible(assign2TitleLabel);

    assign2FunctionLabel.setJustificationType(juce::Justification::centredLeft);
    assign2FunctionLabel.setFont(juce::Font(17.0f));
    assign2FunctionLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(assign2FunctionLabel);

    addAndMakeVisible(audioHealthMonitor);

    if (appContext.pluginHostManager != nullptr)
    {
        auto& phm = *appContext.pluginHostManager;

        perfInputVuMeter = std::make_unique<VuMeterComponent>(
            [&phm]()
            { return phm.getChainMeterTaps().preChainPeak.load(std::memory_order_relaxed); },
            nullptr,
            true);
        perfInputVuMeter->setCaption("IN");
        addAndMakeVisible(*perfInputVuMeter);

        perfOutputVuMeter = std::make_unique<VuMeterComponent>(
            [&phm]()
            { return phm.getChainMeterTaps().postOutputGainPeak.load(std::memory_order_relaxed); },
            nullptr,
            true);
        perfOutputVuMeter->setCaption("OUT");
        addAndMakeVisible(*perfOutputVuMeter);
    }

    refreshHud();
    startTimerHz(12);
}

PerformanceViewComponent::~PerformanceViewComponent() = default;

void PerformanceViewComponent::paint(juce::Graphics& g)
{
    g.fillAll(perfBg());
}

void PerformanceViewComponent::timerCallback()
{
    refreshHud();
}

void PerformanceViewComponent::syncEncoderFocus()
{
    if (appContext.encoderNavigator == nullptr || !isShowing())
        return;

    if (appContext.projectSceneJumpBrowserOpen)
        return;

    if (NameEntryModal::isAnyActiveInstanceVisible() || UnsavedChangesModal::isAnyActiveInstanceVisible())
        return;

    std::vector<EncoderFocusItem> items;

    items.push_back({ &rackEditButton, [this]() { rackEditButton.triggerClick(); }, {} });
    items.push_back({ &scenePrevButton, [this]() { scenePrevButton.triggerClick(); }, {} });
    items.push_back({ &sceneNextButton, [this]() { sceneNextButton.triggerClick(); }, {} });
    items.push_back({ &chainPrevButton, [this]() { chainPrevButton.triggerClick(); }, {} });
    items.push_back({ &chainNextButton, [this]() { chainNextButton.triggerClick(); }, {} });

    for (size_t i = 0; i < knobCards.size(); ++i)
    {
        if (knobCards[i] != nullptr)
            items.push_back({ knobCards[i].get(), [] {}, {} });
    }

    items.push_back({ &assign1FunctionLabel, [] {}, {} });
    items.push_back({ &assign2FunctionLabel, [] {}, {} });

    items.push_back({ &settingsButton, [this]() { settingsButton.triggerClick(); }, {} });

    appContext.encoderNavigator->setRootFocusChain(std::move(items));
}

void PerformanceViewComponent::refreshHud()
{
    const NavigationStatus nav = computeNavigationStatus(appContext);

    const juce::String sceneDisplay =
        nav.hasActiveScene() && nav.sceneName.isNotEmpty() ? nav.sceneName
                                                           : juce::String("Untitled scene");

    const juce::String bpmLine =
        nav.hasActiveScene() ? juce::String(nav.tempoBpm, 1) + " BPM" : juce::String("- BPM");

    const ParameterMappingDescriptor* mapKnob[4] = {};

    if (appContext.sceneManager != nullptr)
    {
        const bool multiScene = nav.sceneCount > 1;
        scenePrevButton.setEnabled(multiScene);
        sceneNextButton.setEnabled(multiScene);
        sceneCountLabel.setText(nav.getSceneCountSummary(), juce::dontSendNotification);

        chainPrevButton.setEnabled(nav.chainCount > 0);
        chainNextButton.setEnabled(nav.chainCount > 0);

        juce::Array<ParameterMappingDescriptor> mappingRows;

        if (appContext.parameterMappingManager != nullptr)
            mappingRows = appContext.parameterMappingManager->getAllMappings();

        for (int k = 0; k < 4; ++k)
        {
            const auto hid = static_cast<HardwareControlId>(static_cast<int>(HardwareControlId::Knob1) + k);
            mapKnob[k] = findMappingFor(mappingRows, nav.sceneId, nav.chainId, hid);
        }

        const ParameterMappingDescriptor* mapA1 =
            findMappingFor(mappingRows, nav.sceneId, nav.chainId, HardwareControlId::AssignButton1);
        const ParameterMappingDescriptor* mapA2 =
            findMappingFor(mappingRows, nav.sceneId, nav.chainId, HardwareControlId::AssignButton2);

        assign1FunctionLabel.setText(mapA1 != nullptr && mapA1->displayName.isNotEmpty() ? mapA1->displayName
                                                                                         : juce::String("-"),
                                   juce::dontSendNotification);

        assign2FunctionLabel.setText(mapA2 != nullptr && mapA2->displayName.isNotEmpty() ? mapA2->displayName
                                                                                         : juce::String("-"),
                                   juce::dontSendNotification);
    }
    else
    {
        scenePrevButton.setEnabled(false);
        sceneNextButton.setEnabled(false);
        sceneCountLabel.setText("-", juce::dontSendNotification);
        chainPrevButton.setEnabled(false);
        chainNextButton.setEnabled(false);
    }

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

    projectNameLabel.setText(nav.getProjectHeaderLine(), juce::dontSendNotification);
    heroSceneLabel.setText(sceneDisplay, juce::dontSendNotification);
    chainHeaderLabel.setText(nav.getChainDisplayLabel(), juce::dontSendNotification);
    bpmStatusLabel.setText(bpmLine, juce::dontSendNotification);
    chainCountLabel.setText(nav.getChainCountSummary(), juce::dontSendNotification);

    if (appContext.parameterMappingManager != nullptr)
    {
        for (int k = 0; k < 4; ++k)
        {
            if (knobCards[static_cast<size_t>(k)] == nullptr)
                continue;

            if (mapKnob[k] == nullptr)
            {
                knobCards[static_cast<size_t>(k)]->setParameterTitle("K"
                                                                     + juce::String(k + 1)
                                                                     + " Unassigned");
                knobCards[static_cast<size_t>(k)]->setNormalized(0.0f);
                knobCards[static_cast<size_t>(k)]->setValueText("--");
                continue;
            }

            float pluginNorm01 = 0.0f;

            if (! appContext.parameterMappingManager->tryReadMappedParameterNormalized(*mapKnob[k], pluginNorm01))
            {
                knobCards[static_cast<size_t>(k)]->setParameterTitle(mapKnob[k]->displayName.isNotEmpty()
                                                                          ? mapKnob[k]->displayName
                                                                          : juce::String("K") + juce::String(k + 1));
                knobCards[static_cast<size_t>(k)]->setNormalized(0.0f);
                knobCards[static_cast<size_t>(k)]->setValueText("--");
                continue;
            }

            const float arc =
                ParameterMappingManager::hardwareArc01ForHud(*mapKnob[k], pluginNorm01);

            knobCards[static_cast<size_t>(k)]->setNormalized(arc);

            juce::String title =
                mapKnob[k]->displayName.isNotEmpty()
                    ? mapKnob[k]->displayName
                    : juce::String("K") + juce::String(k + 1);

            knobCards[static_cast<size_t>(k)]->setParameterTitle(title);

            juce::String valueLine = juce::String(juce::roundToInt(arc * 100.0f)) + "%";
            const juce::String hostText =
                appContext.parameterMappingManager->getMappedParameterValueText(*mapKnob[k]);

            if (hostText.isNotEmpty())
                valueLine = hostText;

            knobCards[static_cast<size_t>(k)]->setValueText(valueLine);
        }

        const juce::String logKey = nav.sceneId + "|" + nav.chainId;

        if (logKey != lastAssignablesHudLogKey)
        {
            lastAssignablesHudLogKey = logKey;

            Logger::info("FORGE7 Assignables: refreshing display from active chain");

            juce::Array<ParameterMappingDescriptor> rowsForLog =
                appContext.parameterMappingManager->getAllMappings();

            auto* pluginChain =
                appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

            for (int k = 0; k < 4; ++k)
            {
                const auto hid =
                    static_cast<HardwareControlId>(static_cast<int>(HardwareControlId::Knob1) + k);
                const auto* row = findMappingFor(rowsForLog, nav.sceneId, nav.chainId, hid);

                if (row == nullptr)
                {
                    Logger::info("FORGE7 Assignables: K" + juce::String(k + 1) + " unassigned");
                    continue;
                }

                float v = 0.0f;
                appContext.parameterMappingManager->tryReadMappedParameterNormalized(*row, v);

                juce::String plugName = "-";

                if (pluginChain != nullptr)
                    if (auto* sl = pluginChain->getSlot(static_cast<size_t>(row->pluginSlotIndex)))
                        plugName = sl->getPluginDescription().name;

                Logger::info("FORGE7 Assignables: K" + juce::String(k + 1) + " mapped plugin=\"" + plugName
                             + "\" param=\"" + row->displayName + "\" value=" + juce::String(v, 3));
            }
        }
    }
    else
    {
        for (size_t k = 0; k < knobCards.size(); ++k)
        {
            if (knobCards[k] == nullptr)
                continue;

            knobCards[k]->setParameterTitle("K" + juce::String(static_cast<int>(k) + 1) + " Unassigned");
            knobCards[k]->setNormalized(0.0f);
            knobCards[k]->setValueText("--");
        }
    }

    syncEncoderFocus();
}

void PerformanceViewComponent::resized()
{
    auto r = getLocalBounds();

    const int topBarH = 48;
    const int bottomHud = 26;
    const int sideMeterW = 18;

    auto top = r.removeFromTop(topBarH).reduced(8, 6);

    const int primaryBtn = juce::jmin(100, top.getWidth() / 6);
    rackEditButton.setBounds(top.removeFromLeft(primaryBtn));
    top.removeFromLeft(8);
    settingsButton.setBounds(top.removeFromLeft(primaryBtn));

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    top.removeFromLeft(8);
    simHwButton.setBounds(top.removeFromLeft(juce::jmin(92, primaryBtn + 18)));
#endif

    top.removeFromLeft(juce::jmax(12, top.getWidth() / 10));

    const int statW = juce::jmin(104, juce::jmax(76, top.getWidth() / 6));
    bpmStatusLabel.setBounds(top.removeFromRight(statW));
    cpuMeter.setBounds(top.removeFromRight(statW).reduced(2, 0));

 #if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    // Small dev hint under top bar, left aligned with content.
    simHwHintLabel.setBounds(r.removeFromTop(18).reduced(12, 0));
 #endif

    auto bottomArea = r.removeFromBottom(bottomHud);
    audioHealthMonitor.setBounds(bottomArea.reduced(12, 4));

    if (perfInputVuMeter != nullptr)
        perfInputVuMeter->setBounds(0, topBarH, sideMeterW, juce::jmax(0, getHeight() - topBarH - bottomHud));
    if (perfOutputVuMeter != nullptr)
        perfOutputVuMeter->setBounds(juce::jmax(0, getWidth() - sideMeterW),
                                       topBarH,
                                       sideMeterW,
                                       juce::jmax(0, getHeight() - topBarH - bottomHud));

    r.removeFromLeft(sideMeterW);
    r.removeFromRight(sideMeterW);

    r.reduce(14, 10);

    projectNameLabel.setBounds(r.removeFromTop(18));
    r.removeFromTop(1);
    projectDirtyLabel.setBounds(r.removeFromTop(16));
    r.removeFromTop(2);
    heroSceneLabel.setBounds(r.removeFromTop(48));
    r.removeFromTop(4);

    auto sceneRow = r.removeFromTop(40);
    const int sceneBtnW = juce::jmin(88, sceneRow.getWidth() / 7);
    scenePrevButton.setBounds(sceneRow.removeFromLeft(sceneBtnW).reduced(0, 4));
    sceneRow.removeFromLeft(6);
    sceneNextButton.setBounds(sceneRow.removeFromRight(sceneBtnW).reduced(0, 4));
    sceneRow.removeFromRight(6);
    sceneCountLabel.setBounds(sceneRow.reduced(4, 6));

    r.removeFromTop(4);
    chainHeaderLabel.setBounds(r.removeFromTop(32));

    auto chainRow = r.removeFromTop(44);
    const int chainBtnW = juce::jmin(96, chainRow.getWidth() / 6);
    chainPrevButton.setBounds(chainRow.removeFromLeft(chainBtnW).reduced(0, 4));
    chainRow.removeFromLeft(8);
    chainNextButton.setBounds(chainRow.removeFromRight(chainBtnW).reduced(0, 4));
    chainRow.removeFromRight(8);
    chainCountLabel.setBounds(chainRow.reduced(4, 6));

    r.removeFromTop(12);

    const int knobH = juce::jmax(152, juce::jmin(210, r.getHeight() / 2));
    auto knobRow = r.removeFromTop(knobH);
    const int gap = 8;
    const int kw = juce::jmax(88, (knobRow.getWidth() - gap * 3) / 4);

    for (int i = 0; i < 4; ++i)
    {
        if (knobCards[static_cast<size_t>(i)] != nullptr)
        {
            knobCards[static_cast<size_t>(i)]->setBounds(knobRow.removeFromLeft(kw).reduced(2, 0));
            if (i < 3)
                knobRow.removeFromLeft(gap);
        }
    }

    r.removeFromTop(12);

    const int assignH = 56;
    auto assignRow = r.removeFromTop(assignH);

    const int half = juce::jmax(130, assignRow.getWidth() / 2 - 8);
    auto a1 = assignRow.removeFromLeft(half).reduced(4, 2);
    auto a1Head = a1.removeFromTop(18);

    if (assignButton1Led != nullptr)
        assignButton1Led->setBounds(a1Head.removeFromLeft(14).withSizeKeepingCentre(11, 11));

    assign1TitleLabel.setBounds(a1Head);

    assign1FunctionLabel.setBounds(a1);

    assignRow.removeFromLeft(8);

    auto a2 = assignRow.removeFromLeft(half).reduced(4, 2);
    auto a2Head = a2.removeFromTop(18);

    if (assignButton2Led != nullptr)
        assignButton2Led->setBounds(a2Head.removeFromLeft(14).withSizeKeepingCentre(11, 11));

    assign2TitleLabel.setBounds(a2Head);
    assign2FunctionLabel.setBounds(a2);

    syncEncoderFocus();
}

} // namespace forge7
