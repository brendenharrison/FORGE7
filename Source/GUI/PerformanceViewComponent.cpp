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
#include "../Scene/ChainVariation.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"

namespace forge7
{
namespace
{
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
    simHwHintLabel.setText("Sim HW: use in-app drawer for K1-K4 / assigns / chain / encoder", juce::dontSendNotification);
    addAndMakeVisible(simHwHintLabel);
#endif

    rackEditButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->setEditMode(true);
    };

    chainPrevButton.onClick = [this]()
    {
        if (appContext.sceneManager != nullptr && appContext.pluginHostManager != nullptr)
            appContext.sceneManager->previousChainVariationWithCrossfade(*appContext.pluginHostManager);
    };

    chainNextButton.onClick = [this]()
    {
        if (appContext.sceneManager != nullptr && appContext.pluginHostManager != nullptr)
            appContext.sceneManager->nextChainVariationWithCrossfade(*appContext.pluginHostManager);
    };

    settingsButton.onClick = [this]()
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->openSettings();
    };

    addAndMakeVisible(rackEditButton);
    addAndMakeVisible(chainPrevButton);
    addAndMakeVisible(chainNextButton);
    addAndMakeVisible(settingsButton);

    styleHudLabel(bpmStatusLabel, 16.0f, perfText());
    addAndMakeVisible(bpmStatusLabel);

    addAndMakeVisible(cpuMeter);

    heroSceneLabel.setJustificationType(juce::Justification::centredLeft);
    heroSceneLabel.setFont(juce::Font(34.0f));
    heroSceneLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(heroSceneLabel);

    variationLabel.setJustificationType(juce::Justification::centredLeft);
    variationLabel.setFont(juce::Font(22.0f));
    variationLabel.setColour(juce::Label::textColourId, perfAccent());
    addAndMakeVisible(variationLabel);

    chainVarIndexLabel.setJustificationType(juce::Justification::centred);
    chainVarIndexLabel.setFont(juce::Font(16.0f));
    chainVarIndexLabel.setColour(juce::Label::textColourId, perfMuted());
    addAndMakeVisible(chainVarIndexLabel);

    for (size_t i = 0; i < knobCards.size(); ++i)
    {
        knobCards[i] = std::make_unique<KnobCard>();
        knobCards[i]->setParameterTitle("K" + juce::String(static_cast<int>(i) + 1));
        addAndMakeVisible(*knobCards[i]);
    }

    assign1TitleLabel.setText("Assign 1", juce::dontSendNotification);
    styleHudLabel(assign1TitleLabel, 13.0f, perfMuted());
    addAndMakeVisible(assign1TitleLabel);

    assign1FunctionLabel.setJustificationType(juce::Justification::centredLeft);
    assign1FunctionLabel.setFont(juce::Font(17.0f));
    assign1FunctionLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(assign1FunctionLabel);

    assign2TitleLabel.setText("Assign 2", juce::dontSendNotification);
    styleHudLabel(assign2TitleLabel, 13.0f, perfMuted());
    addAndMakeVisible(assign2TitleLabel);

    assign2FunctionLabel.setJustificationType(juce::Justification::centredLeft);
    assign2FunctionLabel.setFont(juce::Font(17.0f));
    assign2FunctionLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(assign2FunctionLabel);

    addAndMakeVisible(audioHealthMonitor);

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

    std::vector<EncoderFocusItem> items;

    items.push_back({ &rackEditButton, [this]() { rackEditButton.triggerClick(); }, {} });
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
    juce::String sceneName { "Scene" };
    juce::String variationName { "Variation" };
    juce::String bpmLine { "- BPM" };

    juce::String activeSceneId;
    juce::String activeVarId;

    const ParameterMappingDescriptor* mapKnob[4] = {};

    int variationIndexDisplay = -1;
    int variationCountDisplay = 0;

    if (appContext.sceneManager != nullptr)
    {
        const auto& scenes = appContext.sceneManager->getScenes();
        const int activeIdx = appContext.sceneManager->getActiveSceneIndex();

        if (juce::isPositiveAndBelow(activeIdx, static_cast<int>(scenes.size())) && scenes[static_cast<size_t>(activeIdx)] != nullptr)
        {
            const auto& sc = *scenes[static_cast<size_t>(activeIdx)];
            sceneName = sc.getSceneName().isNotEmpty() ? sc.getSceneName() : juce::String("Untitled scene");
            activeSceneId = sc.getSceneId();

            bpmLine = juce::String(sc.getTempoBpm(), 1) + " BPM";

            const auto& vars = sc.getVariations();
            const int rawVi = sc.getActiveChainVariationIndex();
            const int vi =
                vars.empty() ? 0
                             : juce::jlimit(0, static_cast<int>(vars.size()) - 1, rawVi);

            variationCountDisplay = static_cast<int>(vars.size());

            if (juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) && vars[static_cast<size_t>(vi)] != nullptr)
            {
                const auto& v = *vars[static_cast<size_t>(vi)];
                variationName = v.getVariationName().isNotEmpty() ? v.getVariationName() : juce::String("Variation");
                activeVarId = v.getVariationId();
                variationIndexDisplay = vi;
            }

            chainPrevButton.setEnabled(variationIndexDisplay > 0);
            chainNextButton.setEnabled(variationCountDisplay > 0 && variationIndexDisplay < variationCountDisplay - 1);
        }
        else
        {
            chainPrevButton.setEnabled(false);
            chainNextButton.setEnabled(false);
        }

        juce::Array<ParameterMappingDescriptor> mappingRows;

        if (appContext.parameterMappingManager != nullptr)
            mappingRows = appContext.parameterMappingManager->getAllMappings();

        for (int k = 0; k < 4; ++k)
        {
            const auto hid = static_cast<HardwareControlId>(static_cast<int>(HardwareControlId::Knob1) + k);
            mapKnob[k] = findMappingFor(mappingRows, activeSceneId, activeVarId, hid);
        }

        const ParameterMappingDescriptor* mapA1 =
            findMappingFor(mappingRows, activeSceneId, activeVarId, HardwareControlId::AssignButton1);
        const ParameterMappingDescriptor* mapA2 =
            findMappingFor(mappingRows, activeSceneId, activeVarId, HardwareControlId::AssignButton2);

        assign1FunctionLabel.setText(mapA1 != nullptr && mapA1->displayName.isNotEmpty() ? mapA1->displayName
                                                                                         : juce::String("-"),
                                   juce::dontSendNotification);

        assign2FunctionLabel.setText(mapA2 != nullptr && mapA2->displayName.isNotEmpty() ? mapA2->displayName
                                                                                         : juce::String("-"),
                                   juce::dontSendNotification);
    }

    heroSceneLabel.setText(sceneName, juce::dontSendNotification);
    variationLabel.setText(variationName, juce::dontSendNotification);
    bpmStatusLabel.setText(bpmLine, juce::dontSendNotification);

    if (variationCountDisplay > 0 && variationIndexDisplay >= 0)
        chainVarIndexLabel.setText(juce::String(variationIndexDisplay + 1) + " / "
                                       + juce::String(variationCountDisplay),
                                   juce::dontSendNotification);
    else
        chainVarIndexLabel.setText("-", juce::dontSendNotification);

    if (appContext.controlManager != nullptr)
    {
        const auto& hw = appContext.controlManager->getHardwareState();

        for (int k = 0; k < 4; ++k)
        {
            if (knobCards[static_cast<size_t>(k)] == nullptr)
                continue;

            const float norm = hw.getKnobNormalized(k);
            knobCards[static_cast<size_t>(k)]->setNormalized(norm);

            juce::String title = "K" + juce::String(k + 1);

            if (mapKnob[k] != nullptr && mapKnob[k]->displayName.isNotEmpty())
                title = mapKnob[k]->displayName;

            knobCards[static_cast<size_t>(k)]->setParameterTitle(title);

            const int pct = juce::roundToInt(norm * 100.0f);
            knobCards[static_cast<size_t>(k)]->setValueText(juce::String(pct) + "%");
        }
    }

    syncEncoderFocus();
}

void PerformanceViewComponent::resized()
{
    auto r = getLocalBounds();

    const int topBarH = 48;
    const int bottomHud = 26;

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

    r.reduce(14, 10);

    heroSceneLabel.setBounds(r.removeFromTop(52));
    r.removeFromTop(6);
    variationLabel.setBounds(r.removeFromTop(34));

    auto chainRow = r.removeFromTop(44);
    const int chainBtnW = juce::jmin(72, chainRow.getWidth() / 8);
    chainPrevButton.setBounds(chainRow.removeFromLeft(chainBtnW).reduced(0, 4));
    chainRow.removeFromLeft(8);
    chainNextButton.setBounds(chainRow.removeFromRight(chainBtnW).reduced(0, 4));
    chainRow.removeFromRight(8);
    chainVarIndexLabel.setBounds(chainRow.reduced(4, 6));

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
    assign1TitleLabel.setBounds(a1.removeFromTop(18));
    assign1FunctionLabel.setBounds(a1);

    assignRow.removeFromLeft(8);

    auto a2 = assignRow.removeFromLeft(half).reduced(4, 2);
    assign2TitleLabel.setBounds(a2.removeFromTop(18));
    assign2FunctionLabel.setBounds(a2);

    syncEncoderFocus();
}

} // namespace forge7
