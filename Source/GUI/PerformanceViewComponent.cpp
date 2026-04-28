#include "PerformanceViewComponent.h"

#include <vector>

#include "../App/AppContext.h"

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
        g.drawFittedText(title.isNotEmpty() ? title : juce::String("—"),
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
        g.drawFittedText(valueText.isNotEmpty() ? valueText : juce::String("—"),
                           getLocalBounds().removeFromBottom(28).reduced(8, 0),
                           juce::Justification::centred,
                           1);
    }

private:
    juce::String title;
    juce::String valueText { "—" };
    float normalized { 0.0f };
};

//==============================================================================
PerformanceViewComponent::PerformanceViewComponent(AppContext& context)
    : appContext(context),
      cpuMeter(appContext.audioEngine),
      inputLevelMeter(appContext.audioEngine, MeterChannel::inputAfterGain),
      outputLevelMeter(appContext.audioEngine, MeterChannel::outputAfterGain),
      audioHealthMonitor(appContext.audioEngine)
{
    addAndMakeVisible(tunerButton);
    addAndMakeVisible(tempoButton);
    addAndMakeVisible(setlistButton);
    addAndMakeVisible(settingsButton);
    styleToolbarButton(tunerButton);
    styleToolbarButton(tempoButton);
    styleToolbarButton(setlistButton);
    styleToolbarButton(settingsButton);

    tunerButton.onClick = []()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                 "Tuner",
                                                 "Tuner — coming soon.",
                                                 "OK");
    };

    tempoButton.onClick = []()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                 "Tempo",
                                                 "Tap tempo / BPM edit — coming soon.",
                                                 "OK");
    };

    setlistButton.onClick = []()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                 "Setlist",
                                                 "Setlist navigation — coming soon.",
                                                 "OK");
    };

    settingsButton.onClick = []()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                 "Settings",
                                                 "Global settings — coming soon.",
                                                 "OK");
    };

    styleHudLabel(bpmStatusLabel, 16.0f, perfText());
    addAndMakeVisible(bpmStatusLabel);

    addAndMakeVisible(cpuMeter);
    addAndMakeVisible(inputLevelMeter);
    addAndMakeVisible(outputLevelMeter);

    songTitleLabel.setJustificationType(juce::Justification::centredLeft);
    songTitleLabel.setFont(juce::Font(18.0f));
    songTitleLabel.setColour(juce::Label::textColourId, perfMuted());
    songTitleLabel.setText("Song", juce::dontSendNotification);
    addAndMakeVisible(songTitleLabel);

    heroSceneLabel.setJustificationType(juce::Justification::centredLeft);
    heroSceneLabel.setFont(juce::Font(38.0f));
    heroSceneLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(heroSceneLabel);

    styleHudLabel(sceneDetailLabel, 15.0f, perfMuted());
    addAndMakeVisible(sceneDetailLabel);

    variationLabel.setJustificationType(juce::Justification::centredLeft);
    variationLabel.setFont(juce::Font(20.0f));
    variationLabel.setColour(juce::Label::textColourId, perfAccent());
    addAndMakeVisible(variationLabel);

    for (size_t i = 0; i < knobCards.size(); ++i)
    {
        knobCards[i] = std::make_unique<KnobCard>();
        knobCards[i]->setParameterTitle("K" + juce::String(static_cast<int>(i) + 1));
        addAndMakeVisible(*knobCards[i]);
    }

    assign1TitleLabel.setText("Assign 1", juce::dontSendNotification);
    styleHudLabel(assign1TitleLabel, 14.0f, perfMuted());
    addAndMakeVisible(assign1TitleLabel);

    assign1FunctionLabel.setJustificationType(juce::Justification::centredLeft);
    assign1FunctionLabel.setFont(juce::Font(18.0f));
    assign1FunctionLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(assign1FunctionLabel);

    assign2TitleLabel.setText("Assign 2", juce::dontSendNotification);
    styleHudLabel(assign2TitleLabel, 14.0f, perfMuted());
    addAndMakeVisible(assign2TitleLabel);

    assign2FunctionLabel.setJustificationType(juce::Justification::centredLeft);
    assign2FunctionLabel.setFont(juce::Font(18.0f));
    assign2FunctionLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(assign2FunctionLabel);

    chainStatusLabel.setJustificationType(juce::Justification::centredLeft);
    chainStatusLabel.setFont(juce::Font(17.0f));
    chainStatusLabel.setColour(juce::Label::textColourId, perfText());
    addAndMakeVisible(chainStatusLabel);

    scenesSectionLabel.setText("Scenes", juce::dontSendNotification);
    styleHudLabel(scenesSectionLabel, 12.0f, perfMuted());
    scenesSectionLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(scenesSectionLabel);

    for (auto& lab : sceneListLabels)
    {
        lab.setJustificationType(juce::Justification::centredLeft);
        lab.setFont(juce::Font(14.0f));
        lab.setColour(juce::Label::textColourId, perfMuted());
        addAndMakeVisible(lab);
    }

    addAndMakeVisible(audioHealthMonitor);

    refreshHud();
    startTimerHz(12);
}

PerformanceViewComponent::~PerformanceViewComponent() = default;

void PerformanceViewComponent::paint(juce::Graphics& g)
{
    g.fillAll(perfBg());

    auto full = getLocalBounds();
    auto belowTop = full.withTop(full.getY() + 52).withBottom(full.getBottom());
    auto leftStrip = belowTop.removeFromLeft(132).reduced(6, 8);

    g.setColour(perfPanel().withAlpha(0.92f));
    g.fillRoundedRectangle(leftStrip.toFloat(), 8.0f);
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

    if (appContext.sceneManager != nullptr)
    {
        const auto& scenes = appContext.sceneManager->getScenes();
        const int nScenes = static_cast<int>(scenes.size());
        const int activeIdx = appContext.sceneManager->getActiveSceneIndex();
        const int centre = juce::jlimit(0, juce::jmax(0, nScenes - 1), activeIdx);

        for (int row = 0; row < kMaxSceneListRows; ++row)
        {
            const int si = centre - 2 + row;

            if (!juce::isPositiveAndBelow(si, nScenes) || scenes[static_cast<size_t>(si)] == nullptr)
                continue;

            auto& lab = sceneListLabels[static_cast<size_t>(row)];

            if (!lab.isVisible())
                continue;

            items.push_back({ &lab,
                             [this, si]()
                             {
                                 if (appContext.sceneManager == nullptr)
                                     return;

                                 appContext.sceneManager->selectScene(si);

                                 if (appContext.pluginHostManager != nullptr)
                                     appContext.pluginHostManager->commitChainVariationCrossfade(*appContext.sceneManager);
                             } });
        }
    }

    for (size_t i = 0; i < knobCards.size(); ++i)
    {
        if (knobCards[i] != nullptr)
            items.push_back({ knobCards[i].get(), [] {}, {} });
    }

    items.push_back({ &tunerButton, [this]() { tunerButton.triggerClick(); }, {} });
    items.push_back({ &tempoButton, [this]() { tempoButton.triggerClick(); }, {} });
    items.push_back({ &setlistButton, [this]() { setlistButton.triggerClick(); }, {} });
    items.push_back({ &settingsButton, [this]() { settingsButton.triggerClick(); }, {} });

    appContext.encoderNavigator->setRootFocusChain(std::move(items));
}

void PerformanceViewComponent::refreshHud()
{
    juce::String sceneName { "Scene" };
    juce::String sceneIdStr { "—" };
    juce::String variationName { "Variation" };
    juce::String bpmLine { "— BPM" };
    juce::String chainLine { "Chain —" };

    juce::String activeSceneId;
    juce::String activeVarId;

    const ParameterMappingDescriptor* mapKnob[4] = {};

    if (appContext.sceneManager != nullptr)
    {
        const auto& scenes = appContext.sceneManager->getScenes();
        const int activeIdx = appContext.sceneManager->getActiveSceneIndex();

        if (juce::isPositiveAndBelow(activeIdx, static_cast<int>(scenes.size())) && scenes[static_cast<size_t>(activeIdx)] != nullptr)
        {
            const auto& sc = *scenes[static_cast<size_t>(activeIdx)];
            sceneIdStr = sc.getSceneId().isNotEmpty() ? sc.getSceneId() : juce::String("—");
            sceneName = sc.getSceneName().isNotEmpty() ? sc.getSceneName() : juce::String("Untitled scene");
            activeSceneId = sc.getSceneId();

            bpmLine = juce::String(sc.getTempoBpm(), 1) + " BPM";

            const auto& vars = sc.getVariations();
            const int rawVi = sc.getActiveChainVariationIndex();
            const int vi =
                vars.empty() ? 0
                             : juce::jlimit(0, static_cast<int>(vars.size()) - 1, rawVi);

            if (juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) && vars[static_cast<size_t>(vi)] != nullptr)
            {
                const auto& v = *vars[static_cast<size_t>(vi)];
                variationName = v.getVariationName().isNotEmpty() ? v.getVariationName() : juce::String("Variation");
                activeVarId = v.getVariationId();

                const int n = static_cast<int>(vars.size());
                const bool hasPrev = vi > 0;
                const bool hasNext = vi < n - 1;

                chainLine = "Chain " + juce::String(vi + 1) + "/" + juce::String(n) + " | ";
                chainLine += hasPrev ? juce::String("< prev | ") : juce::String("");
                chainLine += variationName;
                chainLine += hasNext ? juce::String(" | next >") : juce::String("");
            }
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
                                                                                         : juce::String("Unassigned"),
                                   juce::dontSendNotification);

        assign2FunctionLabel.setText(mapA2 != nullptr && mapA2->displayName.isNotEmpty() ? mapA2->displayName
                                                                                         : juce::String("Unassigned"),
                                   juce::dontSendNotification);

        const int nScenes = static_cast<int>(scenes.size());
        const int centre = juce::jlimit(0, nScenes - 1, activeIdx);

        for (int row = 0; row < kMaxSceneListRows; ++row)
        {
            const int si = centre - 2 + row;

            if (! juce::isPositiveAndBelow(si, nScenes) || scenes[static_cast<size_t>(si)] == nullptr)
            {
                sceneListLabels[static_cast<size_t>(row)].setVisible(false);
                continue;
            }

            sceneListLabels[static_cast<size_t>(row)].setVisible(true);

            const auto& s = *scenes[static_cast<size_t>(si)];
            juce::String line = s.getSceneName();

            if (line.isEmpty())
                line = "Scene";

            if (si == activeIdx)
            {
                sceneListLabels[static_cast<size_t>(row)].setFont(juce::Font(15.0f).boldened());
                sceneListLabels[static_cast<size_t>(row)].setColour(juce::Label::textColourId, perfHighlight());
                line = "> " + line;
            }
            else
            {
                sceneListLabels[static_cast<size_t>(row)].setFont(juce::Font(14.0f));
                sceneListLabels[static_cast<size_t>(row)].setColour(juce::Label::textColourId, perfMuted());
            }

            sceneListLabels[static_cast<size_t>(row)].setText(line, juce::dontSendNotification);
        }
    }

    heroSceneLabel.setText(sceneName, juce::dontSendNotification);
    sceneDetailLabel.setText("Scene ID  " + sceneIdStr + "   ·   " + sceneName, juce::dontSendNotification);
    variationLabel.setText(variationName, juce::dontSendNotification);
    bpmStatusLabel.setText(bpmLine, juce::dontSendNotification);
    chainStatusLabel.setText(chainLine, juce::dontSendNotification);

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

    const int topBarH = 52;
    const int bottomStrip = 88;
    const int sceneStripW = 132;

    auto top = r.removeFromTop(topBarH).reduced(8, 6);

    const int btnW = juce::jmin(108, top.getWidth() / 10);
    tunerButton.setBounds(top.removeFromLeft(btnW));
    top.removeFromLeft(6);
    tempoButton.setBounds(top.removeFromLeft(btnW));
    top.removeFromLeft(6);
    setlistButton.setBounds(top.removeFromLeft(btnW));
    top.removeFromLeft(6);
    settingsButton.setBounds(top.removeFromLeft(btnW));

    top.removeFromLeft(juce::jmax(8, top.getWidth() / 8));

    const int statW = juce::jmin(100, juce::jmax(72, top.getWidth() / 8));
    bpmStatusLabel.setBounds(top.removeFromRight(statW));
    cpuMeter.setBounds(top.removeFromRight(statW).reduced(2, 0));
    inputLevelMeter.setBounds(top.removeFromRight(statW).reduced(2, 0));
    outputLevelMeter.setBounds(top.removeFromRight(statW).reduced(2, 0));

    auto bottomArea = r.removeFromBottom(bottomStrip);
    audioHealthMonitor.setBounds(bottomArea.reduced(10, 8));

    auto main = r;
    auto sceneStrip = main.removeFromLeft(sceneStripW).reduced(6, 8);

    scenesSectionLabel.setBounds(sceneStrip.removeFromTop(20).reduced(10, 0));

    const int rowH = juce::jmax(22, (sceneStrip.getHeight() - 4) / kMaxSceneListRows);

    for (int i = 0; i < kMaxSceneListRows; ++i)
        sceneListLabels[static_cast<size_t>(i)].setBounds(sceneStrip.removeFromTop(rowH).reduced(10, 2));

    main.reduce(10, 6);

    songTitleLabel.setBounds(main.removeFromTop(22));
    heroSceneLabel.setBounds(main.removeFromTop(52));
    sceneDetailLabel.setBounds(main.removeFromTop(24));
    variationLabel.setBounds(main.removeFromTop(36));

    main.removeFromTop(10);

    const int knobH = juce::jmax(168, juce::jmin(220, main.getHeight() / 2));
    auto knobRow = main.removeFromTop(knobH);
    const int gap = 8;
    const int kw = juce::jmax(96, (knobRow.getWidth() - gap * 3) / 4);

    for (int i = 0; i < 4; ++i)
    {
        if (knobCards[static_cast<size_t>(i)] != nullptr)
        {
            knobCards[static_cast<size_t>(i)]->setBounds(knobRow.removeFromLeft(kw).reduced(2, 0));
            if (i < 3)
                knobRow.removeFromLeft(gap);
        }
    }

    main.removeFromTop(10);

    const int assignH = 52;
    auto assignRow = main.removeFromTop(assignH);

    const int half = juce::jmax(140, assignRow.getWidth() / 2 - 8);
    auto a1 = assignRow.removeFromLeft(half).reduced(4, 2);
    assign1TitleLabel.setBounds(a1.removeFromTop(18));
    assign1FunctionLabel.setBounds(a1);

    assignRow.removeFromLeft(8);

    auto a2 = assignRow.removeFromLeft(half).reduced(4, 2);
    assign2TitleLabel.setBounds(a2.removeFromTop(18));
    assign2FunctionLabel.setBounds(a2);

    main.removeFromTop(6);
    chainStatusLabel.setBounds(main.removeFromTop(28));

    syncEncoderFocus();
}

} // namespace forge7
