#include "ChainControlsPanelComponent.h"

#include "../App/AppContext.h"
#include "../Controls/ParameterMappingDescriptor.h"
#include "../Controls/ParameterMappingManager.h"
#include "HardwareAssignableUi.h"
#include "NavigationStatus.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace forge7
{
namespace
{
constexpr std::array<HardwareControlId, 6> kAssignableHardwareIds = {
    HardwareControlId::Knob1,
    HardwareControlId::Knob2,
    HardwareControlId::Knob3,
    HardwareControlId::Knob4,
    HardwareControlId::AssignButton1,
    HardwareControlId::AssignButton2,
};

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
        setInterceptsMouseClicks(false, false);
        setOpaque(false);

        titleLabel.setText(hardwareDisplayShortName(hid), juce::dontSendNotification);
        titleLabel.setFont(juce::Font(11.0f, juce::Font::bold));

        if (hid == HardwareControlId::AssignButton1)
            titleLabel.setColour(juce::Label::textColourId, button1Colour());
        else if (hid == HardwareControlId::AssignButton2)
            titleLabel.setColour(juce::Label::textColourId, button2Colour());
        else
            titleLabel.setColour(juce::Label::textColourId, textColour());

        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        paramLabel.setFont(juce::Font(10.0f));
        paramLabel.setColour(juce::Label::textColourId, textColour());
        paramLabel.setJustificationType(juce::Justification::centred);
        paramLabel.setMinimumHorizontalScale(0.68f);
        addAndMakeVisible(paramLabel);

        valueLabel.setFont(juce::Font(9.5f));
        valueLabel.setColour(juce::Label::textColourId, mutedColour());
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setMinimumHorizontalScale(0.68f);
        addAndMakeVisible(valueLabel);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(panelBg());
        g.fillRoundedRectangle(bounds, 7.0f);

        auto topAccent = bounds.removeFromTop(3.0f);
        g.setColour(topBarColourForHardware(hid));
        g.fillRoundedRectangle(topAccent, 2.0f);

        bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(panelStroke().withAlpha(0.85f));
        g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
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
        juce::ignoreUnused(pluginName);

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
        auto r = getLocalBounds().reduced(3, 4);
        r.removeFromTop(3); // top accent gap

        const int h = juce::jmax(38, r.getHeight());
        const int titleH = juce::jlimit(12, 15, (h * 26) / 100);
        titleLabel.setBounds(r.removeFromTop(titleH));

        r.removeFromTop(1);
        const int paramH = juce::jmax(16, (r.getHeight() * 55) / 100);
        paramLabel.setBounds(r.removeFromTop(paramH));
        valueLabel.setBounds(r);
    }

private:
    HardwareControlId hid {};
    juce::Label titleLabel;
    juce::Label paramLabel;
    juce::Label valueLabel;
};

} // namespace

//==============================================================================
ChainControlsPanelComponent::ChainControlsPanelComponent(AppContext& context)
    : appContext(context)
{
    setInterceptsMouseClicks(false, false);
    setOpaque(false);

    headingLabel.setText("Control Assignments", juce::dontSendNotification);
    headingLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    headingLabel.setColour(juce::Label::textColourId, textColour());
    headingLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headingLabel);

    for (size_t i = 0; i < stripCells.size(); ++i)
    {
        stripCells[i] = std::make_unique<CompactAssignmentsStripCell>(kAssignableHardwareIds[i]);
        addAndMakeVisible(*stripCells[i]);
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
    refreshStripFromHost();
}

void ChainControlsPanelComponent::refreshFromHost()
{
    refreshStripFromHost();
    repaint();
}

void ChainControlsPanelComponent::refreshStripFromHost()
{
    const NavigationStatus nav = computeNavigationStatus(appContext);

    const bool haveIds = nav.sceneId.isNotEmpty() && nav.chainId.isNotEmpty();
    const auto all = appContext.parameterMappingManager != nullptr ? appContext.parameterMappingManager->getAllMappings()
                                                                   : juce::Array<ParameterMappingDescriptor>();

    auto* chain = appContext.pluginHostManager != nullptr ? appContext.pluginHostManager->getPluginChain() : nullptr;

    for (size_t i = 0; i < stripCells.size(); ++i)
    {
        auto* strip = dynamic_cast<CompactAssignmentsStripCell*>(stripCells[i].get());

        if (strip == nullptr)
            continue;

        const HardwareControlId hid = kAssignableHardwareIds[i];

        if (!haveIds || appContext.parameterMappingManager == nullptr || chain == nullptr)
        {
            strip->setUnassigned();
            continue;
        }

        const auto* row = findMappingForHardware(all, nav.sceneId, nav.chainId, hid);

        if (row == nullptr)
        {
            strip->setUnassigned();
            continue;
        }

        if (! juce::isPositiveAndBelow(row->pluginSlotIndex, PluginChain::getMaxSlots()))
        {
            strip->setMissing("Missing plugin");
            continue;
        }

        auto* slot = chain->getSlot(static_cast<size_t>(row->pluginSlotIndex));
        if (slot == nullptr || slot->isEmptySlot())
        {
            strip->setMissing("Missing plugin");
            continue;
        }

        const juce::String pluginName =
            slot->isPlaceholderOnly() ? juce::String("Missing plugin") : slot->getPluginDescription().name;

        auto* instance = slot->getHostedInstance();
        if (instance == nullptr)
        {
            strip->setMissing("Missing plugin");
            continue;
        }

        auto* param = resolveParameterForRow(*instance, *row);
        if (param == nullptr)
        {
            strip->setMissing("Missing parameter");
            continue;
        }

        const juce::String paramName = param->getName(256);
        const juce::String valueText = appContext.parameterMappingManager->getMappedParameterValueText(*row);

        float pluginNorm01 = 0.0f;
        const bool haveNorm = appContext.parameterMappingManager->tryReadMappedParameterNormalized(*row, pluginNorm01);

        strip->setMapped(*row, pluginName, paramName, valueText, pluginNorm01, haveNorm);
    }
}

void ChainControlsPanelComponent::paint(juce::Graphics& g)
{
    g.setColour(panelBg());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f);

    g.setColour(panelStroke());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.0f);
}

void ChainControlsPanelComponent::resized()
{
    auto r = getLocalBounds().reduced(10, 6);

    constexpr int titleH = 18;
    constexpr int titleGap = 4;

    headingLabel.setBounds(r.removeFromTop(titleH));
    r.removeFromTop(titleGap);

    constexpr int stripGap = 5;
    constexpr int totalCells = 6;

    auto stripBand = r; // remaining height = full strip row (single horizontal row)
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

} // namespace forge7
