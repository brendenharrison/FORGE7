#include "ChainControlsPanelComponent.h"

#include <algorithm>

#include "../App/AppContext.h"
#include "../App/MainComponent.h"
#include "../Controls/ParameterMappingDescriptor.h"
#include "../Controls/ParameterMappingManager.h"
#include "HardwareAssignableUi.h"
#include "NavigationStatus.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"

namespace forge7
{
namespace
{
juce::Colour panelBg() noexcept { return juce::Colour(0xff161a20); }
juce::Colour panelStroke() noexcept { return juce::Colour(0xff4a9eff).withAlpha(0.22f); }
juce::Colour textColour() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour mutedColour() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour accentColour() noexcept { return juce::Colour(0xff4a9eff); }

juce::Colour knobAccentBar() noexcept { return accentColour(); }

juce::Colour topBarColourForHardware(const HardwareControlId hid) noexcept
{
    if (hid == HardwareControlId::AssignButton1)
        return button1Colour();

    if (hid == HardwareControlId::AssignButton2)
        return button2Colour();

    return knobAccentBar();
}

juce::String assignModeLabel(const ParameterMappingDescriptor& row)
{
    if (! isAssignButtonId(row.hardwareControlId))
        return {};

    if (row.toggleMode)
        return "Toggle";

    return "Momentary";
}

const ParameterMappingDescriptor* findMappingForHardware(const juce::Array<ParameterMappingDescriptor>& rows,
                                                        const juce::String& sceneId,
                                                        const juce::String& chainId,
                                                        const HardwareControlId hid)
{
    for (const auto& r : rows)
    {
        if (r.hardwareControlId != hid)
            continue;

        if (r.sceneId == sceneId && r.chainVariationId == chainId)
            return &r;
    }

    return nullptr;
}

juce::AudioProcessorParameter* resolveParameterForRow(juce::AudioProcessor& proc, const ParameterMappingDescriptor& row)
{
    if (row.pluginParameterId.isNotEmpty())
    {
        for (auto* p : proc.getParameters())
        {
            if (p == nullptr)
                continue;

            if (auto* hp = dynamic_cast<juce::HostedAudioProcessorParameter*>(p))
                if (hp->getParameterID() == row.pluginParameterId)
                    return p;
        }
    }

    if (row.pluginParameterIndex >= 0)
    {
        const auto params = proc.getParameters();
        if (row.pluginParameterIndex < params.size())
            return params[static_cast<size_t>(row.pluginParameterIndex)];
    }

    return nullptr;
}

juce::String safeOneLine(const juce::String& s) { return s.replaceCharacters("\r\n\t", "   ").trim(); }

//==============================================================================
class CompactAssignmentsStripCell final : public juce::Component
{
public:
    explicit CompactAssignmentsStripCell(const HardwareControlId hidIn)
        : hid(hidIn)
    {
        setOpaque(false);

        titleLabel.setText(hardwareDisplayShortName(hid), juce::dontSendNotification);
        titleLabel.setFont(juce::Font(12.0f, juce::Font::bold));

        if (hid == HardwareControlId::AssignButton1)
            titleLabel.setColour(juce::Label::textColourId, button1Colour());
        else if (hid == HardwareControlId::AssignButton2)
            titleLabel.setColour(juce::Label::textColourId, button2Colour());
        else
            titleLabel.setColour(juce::Label::textColourId, textColour());

        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        paramLabel.setFont(juce::Font(10.5f));
        paramLabel.setColour(juce::Label::textColourId, textColour());
        paramLabel.setJustificationType(juce::Justification::centred);
        paramLabel.setMinimumHorizontalScale(0.72f);
        addAndMakeVisible(paramLabel);

        valueLabel.setFont(juce::Font(10.0f));
        valueLabel.setColour(juce::Label::textColourId, mutedColour());
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setMinimumHorizontalScale(0.72f);
        addAndMakeVisible(valueLabel);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(panelBg());
        g.fillRoundedRectangle(bounds, 8.0f);

        auto topAccent = bounds.removeFromTop(3.0f);
        g.setColour(topBarColourForHardware(hid));
        g.fillRoundedRectangle(topAccent, 2.0f);

        g.setColour(panelStroke().withAlpha(0.85f));
        bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    }

    void setUnassigned()
    {
        paramLabel.setText("Unassigned", juce::dontSendNotification);
        valueLabel.setText("--", juce::dontSendNotification);
        repaint();
    }

    void setMissing(const juce::String& what)
    {
        paramLabel.setText(what, juce::dontSendNotification);
        valueLabel.setText("--", juce::dontSendNotification);
        repaint();
    }

    void setMapped(const ParameterMappingDescriptor& row,
                   const juce::String& pluginName,
                   const juce::String& parameterName,
                   const juce::String& valueText,
                   float pluginNorm01,
                   bool haveNorm)
    {
        const juce::String mode = assignModeLabel(row);

        const juce::String mapTitle = safeOneLine(row.displayName.isNotEmpty() ? row.displayName : parameterName);

        paramLabel.setText(mapTitle.isNotEmpty() ? mapTitle : juce::String("(mapped)"), juce::dontSendNotification);

        juce::String v;

        if (mode.isEmpty())
        {
            if (haveNorm && valueText.isNotEmpty())
                v = valueText;
            else if (haveNorm)
                v = juce::String(juce::roundToInt(ParameterMappingManager::hardwareArc01ForHud(row, pluginNorm01) * 100.0f))
                    + "%";
            else
                v = "--";
        }
        else
        {
            const juce::String curTxt = valueText.isNotEmpty() ? valueText : juce::String("--");
            v = mode + " | " + curTxt;
        }

        valueLabel.setText(v, juce::dontSendNotification);

        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(4, 5);
        r.removeFromTop(4); // top bar gap

        titleLabel.setBounds(r.removeFromTop(17));

        r.removeFromTop(1);
        paramLabel.setBounds(r.removeFromTop(26));
        valueLabel.setBounds(r.removeFromTop(16));
    }

private:
    HardwareControlId hid {};
    juce::Label titleLabel;
    juce::Label paramLabel;
    juce::Label valueLabel;
};

//==============================================================================
class ControlCard final : public juce::Component
{
public:
    ControlCard(AppContext& ctx, const HardwareControlId hidIn)
        : appContext(ctx)
        , hid(hidIn)
        , barCol(topBarColourForHardware(hidIn))
    {
        setOpaque(false);

        titleLabel.setText(hardwareDisplayShortName(hid), juce::dontSendNotification);
        titleLabel.setFont(juce::Font(15.0f, juce::Font::bold));

        if (hid == HardwareControlId::AssignButton1)
            titleLabel.setColour(juce::Label::textColourId, button1Colour());
        else if (hid == HardwareControlId::AssignButton2)
            titleLabel.setColour(juce::Label::textColourId, button2Colour());
        else
            titleLabel.setColour(juce::Label::textColourId, textColour());

        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        primaryLabel.setFont(juce::Font(14.0f));
        primaryLabel.setColour(juce::Label::textColourId, textColour());
        primaryLabel.setJustificationType(juce::Justification::centredLeft);
        primaryLabel.setMinimumHorizontalScale(0.8f);
        addAndMakeVisible(primaryLabel);

        secondaryLabel.setFont(juce::Font(12.5f));
        secondaryLabel.setColour(juce::Label::textColourId, mutedColour());
        secondaryLabel.setJustificationType(juce::Justification::centredLeft);
        secondaryLabel.setMinimumHorizontalScale(0.75f);
        addAndMakeVisible(secondaryLabel);

        valueLabel.setFont(juce::Font(12.5f));
        valueLabel.setColour(juce::Label::textColourId, mutedColour());
        valueLabel.setJustificationType(juce::Justification::topLeft);
        valueLabel.setMinimumHorizontalScale(0.8f);
        addAndMakeVisible(valueLabel);

        editButton.setButtonText("Edit");
        editButton.setColour(juce::TextButton::buttonColourId, panelBg().brighter(0.12f));
        editButton.setColour(juce::TextButton::textColourOffId, textColour());
        editButton.setVisible(false);
        editButton.onClick = [this]()
        {
            if (mappedSlotIndex < 0)
                return;

            if (appContext.mainComponent != nullptr)
                appContext.mainComponent->openFullscreenPluginEditor(mappedSlotIndex);
        };
        addChildComponent(editButton);
    }

    void paint(juce::Graphics& g) override
    {
        const auto outer = getLocalBounds().toFloat().reduced(2.0f);
        auto body = outer;

        g.setColour(barCol.withAlpha(0.68f));
        g.fillRoundedRectangle(body.removeFromTop(3.5f), 2.5f);

        g.setColour(panelBg());
        g.fillRoundedRectangle(body, 10.0f);

        if (isAssignButtonId(hid))
        {
            auto led = getLocalBounds().reduced(10, 8).removeFromTop(18).removeFromLeft(16).withSizeKeepingCentre(10, 10)
                           .toFloat();
            const juce::Colour ledCol = barCol;
            g.setColour(ledCol.withAlpha(0.25f));
            g.fillEllipse(led.expanded(1.8f));

            g.setColour(ledCol);
            g.fillEllipse(led);
            g.setColour(ledCol.brighter(0.15f));
            g.drawEllipse(led, 1.0f);
        }

        g.setColour(panelStroke());
        g.drawRoundedRectangle(outer, 10.0f, 1.0f);

        // Value bar (simple, compact; not interactive).
        const auto bar = getLocalBounds().reduced(10, 10).removeFromBottom(10).toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(bar, 4.0f);

        g.setColour(barCol.withAlpha(0.85f));
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, arc01)), 4.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10, 8);

        auto top = r.removeFromTop(22);

        if (isAssignButtonId(hid))
            titleLabel.setBounds(top.removeFromLeft(96).withTrimmedLeft(17));
        else
            titleLabel.setBounds(top.removeFromLeft(96));

        editButton.setBounds(top.removeFromRight(56).reduced(0, 2));

        r.removeFromTop(2);
        primaryLabel.setBounds(r.removeFromTop(18));
        secondaryLabel.setBounds(r.removeFromTop(16));
        valueLabel.setBounds(r.removeFromTop(30));
    }

    void setUnassigned()
    {
        mappedSlotIndex = -1;
        primaryLabel.setText("Unassigned", juce::dontSendNotification);
        secondaryLabel.setText("--", juce::dontSendNotification);
        valueLabel.setText("Current: --", juce::dontSendNotification);
        arc01 = 0.0f;
        editButton.setVisible(false);
    }

    void setMissing(const juce::String& what, int slotIndex)
    {
        mappedSlotIndex = slotIndex;
        primaryLabel.setText(what, juce::dontSendNotification);
        secondaryLabel.setText("--", juce::dontSendNotification);
        valueLabel.setText("Current: --", juce::dontSendNotification);
        arc01 = 0.0f;
        editButton.setVisible(mappedSlotIndex >= 0);
    }

    void setMapped(const ParameterMappingDescriptor& row,
                   const juce::String& pluginName,
                   const juce::String& parameterName,
                   const juce::String& valueText,
                   float pluginNorm01,
                   bool haveNorm)
    {
        mappedSlotIndex = row.pluginSlotIndex;

        const juce::String mode = assignModeLabel(row);
        const juce::String mapTitle = safeOneLine(row.displayName.isNotEmpty() ? row.displayName : parameterName);

        primaryLabel.setText(mapTitle.isNotEmpty() ? mapTitle : juce::String("(mapped)"), juce::dontSendNotification);

        const juce::String pname = safeOneLine(parameterName.isNotEmpty() ? parameterName : juce::String("Parameter"));

        secondaryLabel.setText(safeOneLine(pluginName) + " / " + pname, juce::dontSendNotification);

        juce::String vline;

        if (mode.isEmpty())
        {
            if (haveNorm && valueText.isNotEmpty())
                vline = "Chain value: " + valueText;
            else if (haveNorm)
                vline = "Chain value: " + juce::String(juce::roundToInt(ParameterMappingManager::hardwareArc01ForHud(row, pluginNorm01) * 100.0f))
                        + "%";
            else
                vline = "Chain value: --";
        }
        else
        {
            const juce::String curTxt = valueText.isNotEmpty() ? valueText : juce::String("--");
            vline = "Mode: " + mode + "\nChain value: " + curTxt;
        }

        valueLabel.setText(vline, juce::dontSendNotification);

        if (haveNorm && appContext.parameterMappingManager != nullptr)
            arc01 = ParameterMappingManager::hardwareArc01ForHud(row, pluginNorm01);
        else
            arc01 = 0.0f;

        editButton.setVisible(mappedSlotIndex >= 0);
    }

    HardwareControlId getHardwareId() const noexcept { return hid; }

private:
    AppContext& appContext;
    HardwareControlId hid {};
    const juce::Colour barCol;

    juce::Label titleLabel;
    juce::Label primaryLabel;
    juce::Label secondaryLabel;
    juce::Label valueLabel;
    juce::TextButton editButton;

    int mappedSlotIndex { -1 };
    float arc01 { 0.0f };
};

} // namespace

//==============================================================================
ChainControlsPanelComponent::ChainControlsPanelComponent(AppContext& context)
    : appContext(context)
{
    setOpaque(false);

    headingLabel.setText("Control Assignments", juce::dontSendNotification);
    headingLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    headingLabel.setColour(juce::Label::textColourId, textColour());
    headingLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headingLabel);

    for (auto* l : { &sceneLabel, &chainLabel })
    {
        l->setFont(juce::Font(13.0f));
        l->setColour(juce::Label::textColourId, mutedColour());
        l->setJustificationType(juce::Justification::centredLeft);
        l->setMinimumHorizontalScale(0.75f);
        addAndMakeVisible(*l);
    }

    const std::array<HardwareControlId, 6> ids = {
        HardwareControlId::Knob1,
        HardwareControlId::Knob2,
        HardwareControlId::Knob3,
        HardwareControlId::Knob4,
        HardwareControlId::AssignButton1,
        HardwareControlId::AssignButton2,
    };

    for (size_t i = 0; i < stripCells.size(); ++i)
    {
        stripCells[i] = std::make_unique<CompactAssignmentsStripCell>(ids[i]);
        addAndMakeVisible(*stripCells[i]);
    }

    for (size_t i = 0; i < cards.size(); ++i)
    {
        cards[i] = std::make_unique<ControlCard>(appContext, ids[i]);
        addAndMakeVisible(*cards[i]);
    }

    refreshFromHost();
    startTimerHz(8);
}

ChainControlsPanelComponent::~ChainControlsPanelComponent()
{
    stopTimer();
}

void ChainControlsPanelComponent::timerCallback()
{
    refreshCardsFromHost();
}

void ChainControlsPanelComponent::refreshFromHost()
{
    rebuildHeaderText();
    refreshCardsFromHost();
    repaint();
}

void ChainControlsPanelComponent::rebuildHeaderText()
{
    const NavigationStatus nav = computeNavigationStatus(appContext);

    sceneLabel.setText("Scene: " + (nav.sceneName.isNotEmpty() ? nav.sceneName : juce::String("-")),
                       juce::dontSendNotification);

    chainLabel.setText("Chain: " + nav.getChainDisplayLabel(), juce::dontSendNotification);
}

void ChainControlsPanelComponent::refreshCardsFromHost()
{
    const NavigationStatus nav = computeNavigationStatus(appContext);

    const bool haveIds = nav.sceneId.isNotEmpty() && nav.chainId.isNotEmpty();
    const auto all = appContext.parameterMappingManager != nullptr ? appContext.parameterMappingManager->getAllMappings()
                                                                   : juce::Array<ParameterMappingDescriptor>();

    auto* chain = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

    auto updateRow = [&](const size_t idx, const HardwareControlId hid)
    {
        auto* card = dynamic_cast<ControlCard*>(cards[idx].get());
        auto* strip = dynamic_cast<CompactAssignmentsStripCell*>(stripCells[idx].get());

        if (card == nullptr || strip == nullptr)
            return;

        if (!haveIds || appContext.parameterMappingManager == nullptr || chain == nullptr)
        {
            card->setUnassigned();
            strip->setUnassigned();
            return;
        }

        const auto* row = findMappingForHardware(all, nav.sceneId, nav.chainId, hid);

        if (row == nullptr)
        {
            card->setUnassigned();
            strip->setUnassigned();
            return;
        }

        if (! juce::isPositiveAndBelow(row->pluginSlotIndex, PluginChain::getMaxSlots()))
        {
            card->setMissing("Missing plugin", row->pluginSlotIndex);
            strip->setMissing("Missing plugin");
            return;
        }

        auto* slot = chain->getSlot(static_cast<size_t>(row->pluginSlotIndex));
        if (slot == nullptr || slot->isEmptySlot())
        {
            card->setMissing("Missing plugin", row->pluginSlotIndex);
            strip->setMissing("Missing plugin");
            return;
        }

        const juce::String pluginName =
            slot->isPlaceholderOnly() ? juce::String("Missing plugin") : slot->getPluginDescription().name;

        auto* instance = slot->getHostedInstance();
        if (instance == nullptr)
        {
            card->setMissing("Missing plugin", row->pluginSlotIndex);
            strip->setMissing("Missing plugin");
            return;
        }

        auto* param = resolveParameterForRow(*instance, *row);
        if (param == nullptr)
        {
            card->setMissing("Missing parameter", row->pluginSlotIndex);
            strip->setMissing("Missing parameter");
            return;
        }

        const juce::String paramName = param->getName(256);
        const juce::String valueText = appContext.parameterMappingManager->getMappedParameterValueText(*row);

        float pluginNorm01 = 0.0f;
        const bool haveNorm = appContext.parameterMappingManager->tryReadMappedParameterNormalized(*row, pluginNorm01);

        card->setMapped(*row, pluginName, paramName, valueText, pluginNorm01, haveNorm);
        strip->setMapped(*row, pluginName, paramName, valueText, pluginNorm01, haveNorm);
    };

    for (size_t i = 0; i < cards.size(); ++i)
        updateRow(i,
                  static_cast<HardwareControlId>(static_cast<int>(HardwareControlId::Knob1) + static_cast<int>(i)));
}

void ChainControlsPanelComponent::paint(juce::Graphics& g)
{
    g.setColour(panelBg());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 12.0f);

    g.setColour(panelStroke());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 12.0f, 1.0f);
}

void ChainControlsPanelComponent::resized()
{
    auto r = getLocalBounds().reduced(10, 8);

    auto header = r.removeFromTop(50);
    headingLabel.setBounds(header.removeFromTop(20));
    sceneLabel.setBounds(header.removeFromTop(15));
    chainLabel.setBounds(header.removeFromTop(15));

    r.removeFromTop(6);

    constexpr int stripGap = 5;
    constexpr int knobCount = 4;
    constexpr int btnCount = 2;
    constexpr int stripRowH = 64;
    constexpr int stripRowGap = 6;

    const bool wideStrip = getWidth() >= 640;
    const int stripBandH = wideStrip ? stripRowH : (stripRowH + stripRowGap + stripRowH);

    auto stripBand = r.removeFromTop(stripBandH);
    stripBand.removeFromRight(4);

    if (wideStrip)
    {
        // K1 | K2 | K3 | K4 | Button 1 | Button 2 (single horizontal strip)
        const int totalCells = knobCount + btnCount;
        const int w = (stripBand.getWidth() - stripGap * (totalCells - 1)) / totalCells;
        auto row = stripBand;

        for (int i = 0; i < totalCells; ++i)
        {
            if (stripCells[static_cast<size_t>(i)] != nullptr)
                stripCells[static_cast<size_t>(i)]->setBounds(row.removeFromLeft(w));

            if (i < totalCells - 1)
                row.removeFromLeft(stripGap);
        }
    }
    else
    {
        // Two rows: K1-K4, then Button 1 / Button 2
        auto row1 = stripBand.removeFromTop(stripRowH);
        const int w1 = (row1.getWidth() - stripGap * (knobCount - 1)) / knobCount;
        auto r1 = row1;

        for (int i = 0; i < knobCount; ++i)
        {
            if (stripCells[static_cast<size_t>(i)] != nullptr)
                stripCells[static_cast<size_t>(i)]->setBounds(r1.removeFromLeft(w1));

            if (i < knobCount - 1)
                r1.removeFromLeft(stripGap);
        }

        stripBand.removeFromTop(stripRowGap);

        auto row2 = stripBand.removeFromTop(stripRowH);
        const int w2 = (row2.getWidth() - stripGap) / btnCount;
        auto r2 = row2;

        for (int b = 0; b < btnCount; ++b)
        {
            if (stripCells[static_cast<size_t>(knobCount + static_cast<size_t>(b))] != nullptr)
                stripCells[static_cast<size_t>(knobCount + static_cast<size_t>(b))]->setBounds(r2.removeFromLeft(w2));

            if (b < btnCount - 1)
                r2.removeFromLeft(stripGap);
        }
    }

    r.removeFromTop(8);

    const bool wide = getWidth() >= 720;

    if (wide)
    {
        // 2-row grid: K1-K4 then Button 1 / Button 2.
        auto row1 = r.removeFromTop(110);
        row1.removeFromRight(4);
        constexpr int gap = 8;
        const int w = (row1.getWidth() - gap * 3) / 4;

        for (int i = 0; i < 4; ++i)
        {
            if (cards[static_cast<size_t>(i)] != nullptr)
                cards[static_cast<size_t>(i)]->setBounds(row1.removeFromLeft(w));

            if (i < 3)
                row1.removeFromLeft(gap);
        }

        r.removeFromTop(8);

        auto row2 = r.removeFromTop(110);
        const int w2 = (row2.getWidth() - gap) / 2;

        if (cards[4] != nullptr)
            cards[4]->setBounds(row2.removeFromLeft(w2));

        row2.removeFromLeft(gap);

        if (cards[5] != nullptr)
            cards[5]->setBounds(row2.removeFromLeft(w2));
    }
    else
    {
        // Stacked cards.
        constexpr int h = 92;
        constexpr int gap = 8;

        for (auto& c : cards)
        {
            if (c == nullptr)
                continue;

            c->setBounds(r.removeFromTop(h));
            r.removeFromTop(gap);
        }
    }
}

} // namespace forge7
