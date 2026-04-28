#include "RackSlotCard.h"

namespace forge7
{
namespace
{
constexpr int kMinCardWidth = 88;
constexpr int kMinCardHeight = 128;

juce::Colour rackPanel() noexcept { return juce::Colour(0xff252830); }
juce::Colour rackBorder() noexcept { return juce::Colour(0xff3d4450); }
juce::Colour rackAccent() noexcept { return juce::Colour(0xff4a9eff); }
juce::Colour rackText() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour rackMuted() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour rackDanger() noexcept { return juce::Colour(0xffff6b6b); }

juce::Colour thumbPanelFill() noexcept { return juce::Colour(0xff1c1f26); }
juce::Colour thumbHeaderFill() noexcept { return juce::Colour(0xff2a2e38); }
juce::Colour thumbKnobFill() noexcept { return juce::Colour(0xff363b46); }
juce::Colour thumbKnobIndicator() noexcept { return juce::Colour(0xffb0b6c0); }
juce::Colour thumbBadgeFill() noexcept { return juce::Colour(0xff14171d); }

const std::array<juce::Colour, 5> kThumbAccentPalette {
    juce::Colour(0xff4a7fb8), // muted blue
    juce::Colour(0xff3f9b94), // muted teal
    juce::Colour(0xff8169b2), // muted purple
    juce::Colour(0xffc09253), // muted amber
    juce::Colour(0xff5fa372), // muted green
};
} // namespace

RackSlotCard::RackSlotCard()
{
    setInterceptsMouseClicks(true, true);
    setSize(kMinCardWidth, kMinCardHeight);

    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(15.0f));
    titleLabel.setColour(juce::Label::textColourId, rackText());
    addAndMakeVisible(titleLabel);

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, rackMuted());
    addAndMakeVisible(statusLabel);

    bypassButton.setClickingTogglesState(true);
    bypassButton.onClick = [this]()
    {
        if (onBypassChanged != nullptr)
            onBypassChanged(slotIndex, bypassButton.getToggleState());
    };

    bypassButton.setColour(juce::ToggleButton::textColourId, rackText());
    bypassButton.setColour(juce::ToggleButton::tickColourId, rackAccent());
    addAndMakeVisible(bypassButton);

    removeButton.onClick = [this]()
    {
        if (onRemoveRequested != nullptr)
            onRemoveRequested(slotIndex);
    };

    removeButton.setColour(juce::TextButton::buttonColourId, rackPanel());
    removeButton.setColour(juce::TextButton::textColourOffId, rackDanger());
    addAndMakeVisible(removeButton);
}

RackSlotCard::~RackSlotCard() = default;

void RackSlotCard::setSlotIndex(const int index) noexcept
{
    slotIndex = index;
    syncLabelsFromInfo();
}

void RackSlotCard::setSelected(const bool isSelected) noexcept
{
    selected = isSelected;
    repaint();
}

void RackSlotCard::setShowInlineControls(const bool show) noexcept
{
    showInlineControls = show;

    if (!show)
    {
        bypassButton.setVisible(false);
        removeButton.setVisible(false);
    }

    syncLabelsFromInfo();
}

void RackSlotCard::refreshFromSlotInfo(const SlotInfo& info)
{
    lastInfo = info;
    bypassButton.setToggleState(info.bypass, juce::dontSendNotification);
    syncLabelsFromInfo();
    repaint();
}

void RackSlotCard::syncLabelsFromInfo()
{
    titleLabel.setText("Slot " + juce::String(slotIndex + 1), juce::dontSendNotification);

    if (lastInfo.missingPlugin)
    {
        statusLabel.setText("MISSING PLUGIN", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, rackDanger());
        titleLabel.setText(lastInfo.slotDisplayName.isNotEmpty() ? lastInfo.slotDisplayName : "Unknown",
                           juce::dontSendNotification);
    }
    else if (lastInfo.isEmpty)
    {
        statusLabel.setText("Empty", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, rackMuted());
    }
    else if (lastInfo.isPlaceholder)
    {
        statusLabel.setText("Placeholder", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, rackMuted());
        titleLabel.setText(lastInfo.slotDisplayName.isNotEmpty() ? lastInfo.slotDisplayName : "Slot "
                                                                        + juce::String(slotIndex + 1),
                           juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText("Loaded", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, rackMuted());
        titleLabel.setText(lastInfo.slotDisplayName.isNotEmpty() ? lastInfo.slotDisplayName : "Plugin",
                           juce::dontSendNotification);
    }

    if (showInlineControls)
    {
        removeButton.setVisible(!lastInfo.isEmpty || lastInfo.missingPlugin || lastInfo.isPlaceholder);
        bypassButton.setVisible(!lastInfo.isEmpty || lastInfo.isPlaceholder || lastInfo.missingPlugin);
    }
}

void RackSlotCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    const auto fill = lastInfo.missingPlugin ? juce::Colour(0xff3a2020) : rackPanel();
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 8.0f);

    const float thickness = selected ? 3.0f : 1.0f;
    const auto borderCol = selected ? rackAccent() : rackBorder();
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, 8.0f, thickness);

    if (selected)
    {
        g.setColour(rackAccent().withAlpha(0.12f));
        g.fillRoundedRectangle(bounds, 8.0f);
    }

    if (! thumbnailBounds.isEmpty() && ! lastInfo.isEmpty)
        drawPluginThumbnail(g, thumbnailBounds);
}

void RackSlotCard::resized()
{
    auto r = getLocalBounds().reduced(8);

    titleLabel.setBounds(r.removeFromTop(22));
    statusLabel.setBounds(r.removeFromTop(18));

    auto btnRow = r.removeFromBottom(36);
    bypassButton.setBounds(btnRow.removeFromLeft(juce::jmax(100, btnRow.getWidth() / 2)).reduced(0, 2));
    removeButton.setBounds(btnRow.reduced(4, 2));

    auto middle = r.reduced(4, 2);

    constexpr float aspectW = 16.0f;
    constexpr float aspectH = 9.0f;

    if (middle.getWidth() <= 0 || middle.getHeight() <= 0)
    {
        thumbnailBounds = {};
        return;
    }

    int fitW = middle.getWidth();
    int fitH = juce::roundToInt(static_cast<float>(fitW) * (aspectH / aspectW));

    if (fitH > middle.getHeight())
    {
        fitH = middle.getHeight();
        fitW = juce::roundToInt(static_cast<float>(fitH) * (aspectW / aspectH));
    }

    constexpr int kMinThumbHeight = 24;
    if (fitH < kMinThumbHeight || fitW < kMinThumbHeight)
    {
        thumbnailBounds = {};
        return;
    }

    thumbnailBounds = juce::Rectangle<int>(fitW, fitH).withCentre(middle.getCentre());
}

void RackSlotCard::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (onSelect != nullptr)
        onSelect(slotIndex);
}

juce::String RackSlotCard::getPluginInitials(const juce::String& pluginName) const
{
    if (pluginName.isEmpty())
        return "FX";

    juce::String result;
    bool atWordStart = true;

    for (auto ch : pluginName)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(ch))
        {
            if (atWordStart)
            {
                result += juce::String::charToString(juce::CharacterFunctions::toUpperCase(ch));
                atWordStart = false;
                if (result.length() >= 3)
                    break;
            }
        }
        else
        {
            atWordStart = true;
        }
    }

    if (result.isEmpty())
        return pluginName.substring(0, 2).toUpperCase();

    return result;
}

juce::Colour RackSlotCard::getPluginAccentColour(const juce::String& pluginName,
                                                 const juce::String& manufacturer) const
{
    const auto seed = (pluginName + "\n" + manufacturer).hashCode();
    const auto idx = static_cast<size_t>(static_cast<uint32_t>(seed)) % kThumbAccentPalette.size();
    return kThumbAccentPalette[idx];
}

void RackSlotCard::drawPluginThumbnail(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;

    const auto& name = lastInfo.slotDisplayName;
    const auto& ident = lastInfo.pluginIdentifier;
    const bool isMissing = lastInfo.missingPlugin;
    const bool isPlaceholder = lastInfo.isPlaceholder;
    const bool isBypassed = lastInfo.bypass;

    const auto panelRect = area.toFloat();
    constexpr float corner = 9.0f;

    g.setColour(thumbPanelFill());
    g.fillRoundedRectangle(panelRect, corner);

    const auto accent = isMissing ? rackDanger().withMultipliedSaturation(0.55f)
                                  : getPluginAccentColour(name, ident);

    const float headerH = juce::jlimit(10.0f, 20.0f, panelRect.getHeight() * 0.22f);
    const auto headerRect = panelRect.withHeight(headerH);

    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(headerRect.toNearestInt());
        g.setColour(thumbHeaderFill());
        g.fillRoundedRectangle(headerRect.withHeight(headerH + corner), corner);
    }

    auto headerContent = headerRect;
    g.setColour(accent.withAlpha(0.85f));
    g.fillRect(headerContent.removeFromLeft(3.0f).reduced(0.0f, 2.0f));

    const auto headerTextArea = headerContent.reduced(6.0f, 0.0f);
    if (headerTextArea.getWidth() > 6.0f)
    {
        g.setColour(rackText().withAlpha(0.85f));
        g.setFont(juce::Font(juce::jlimit(9.0f, 11.0f, headerH * 0.62f)));
        const auto headerText = name.isNotEmpty() ? name : juce::String("Plugin");
        g.drawFittedText(headerText, headerTextArea.toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.85f);
    }

    auto body = panelRect.withTrimmedTop(headerH);
    const float badgeH = juce::jlimit(10.0f, 16.0f, body.getHeight() * 0.26f);
    auto badgeStrip = body.removeFromBottom(badgeH);

    if (isMissing)
    {
        g.setColour(accent.withAlpha(0.18f));
        g.fillRoundedRectangle(body.reduced(2.0f), 6.0f);

        g.setColour(rackDanger());
        g.setFont(juce::Font(juce::jlimit(11.0f, 16.0f, body.getHeight() * 0.45f), juce::Font::bold));
        g.drawFittedText("MISSING", body.toNearestInt(), juce::Justification::centred, 1, 0.9f);
    }
    else if (isPlaceholder)
    {
        g.setColour(rackMuted().withAlpha(0.18f));
        g.fillRoundedRectangle(body.reduced(2.0f), 6.0f);

        g.setColour(rackMuted());
        g.setFont(juce::Font(juce::jlimit(10.0f, 14.0f, body.getHeight() * 0.4f)));
        g.drawFittedText("Loading...", body.toNearestInt(), juce::Justification::centred, 1, 0.9f);
    }
    else
    {
        const auto seed = static_cast<uint32_t>((name + "\n" + ident).hashCode());
        const int knobCount = 2 + static_cast<int>(seed % 3u); // 2..4
        const bool drawSlider = ((seed >> 3) & 1u) != 0u;

        const float knobsH = juce::jlimit(10.0f, 22.0f, body.getHeight() * 0.42f);
        auto knobsArea = body.removeFromBottom(knobsH);
        auto initialsArea = body;

        g.setColour(accent.withAlpha(0.22f));
        g.fillRoundedRectangle(initialsArea.reduced(2.0f), 5.0f);

        g.setColour(rackText());
        g.setFont(juce::Font(juce::jlimit(12.0f, 22.0f, initialsArea.getHeight() * 0.7f), juce::Font::bold));
        g.drawFittedText(getPluginInitials(name), initialsArea.toNearestInt(),
                         juce::Justification::centred, 1, 0.9f);

        const float knobDiameter = juce::jlimit(6.0f, 14.0f, knobsArea.getHeight() - 4.0f);
        const float spacing = knobsArea.getWidth() / static_cast<float>(knobCount + 1);
        const float cy = knobsArea.getCentreY();

        for (int i = 0; i < knobCount; ++i)
        {
            const float cx = knobsArea.getX() + spacing * static_cast<float>(i + 1);
            const juce::Rectangle<float> knob(cx - knobDiameter * 0.5f,
                                              cy - knobDiameter * 0.5f,
                                              knobDiameter,
                                              knobDiameter);
            g.setColour(thumbKnobFill());
            g.fillEllipse(knob);

            const float angle = static_cast<float>(((seed >> (i * 4)) & 0xFu)) / 15.0f
                              * juce::MathConstants<float>::twoPi;
            const float r = knobDiameter * 0.4f;
            const float ix = knob.getCentreX() + std::cos(angle) * r;
            const float iy = knob.getCentreY() + std::sin(angle) * r;
            g.setColour(accent);
            g.drawLine(knob.getCentreX(), knob.getCentreY(), ix, iy, 1.4f);

            g.setColour(thumbKnobIndicator().withAlpha(0.5f));
            g.drawEllipse(knob, 1.0f);
        }

        if (drawSlider && knobsArea.getWidth() > 16.0f)
        {
            const float trackY = knobsArea.getBottom() - 2.0f;
            g.setColour(rackMuted().withAlpha(0.4f));
            g.drawLine(knobsArea.getX() + 4.0f, trackY,
                       knobsArea.getRight() - 4.0f, trackY, 1.0f);
        }
    }

    {
        const auto badgeText = isMissing ? juce::String("X")
                                         : (isPlaceholder ? juce::String("...") : juce::String("VST3"));
        const float bH = juce::jlimit(9.0f, 14.0f, badgeStrip.getHeight() - 2.0f);
        const float bW = juce::jlimit(22.0f, 36.0f, badgeStrip.getWidth() * 0.4f);
        const juce::Rectangle<float> badge(badgeStrip.getRight() - bW - 4.0f,
                                            badgeStrip.getCentreY() - bH * 0.5f,
                                            bW,
                                            bH);

        g.setColour(thumbBadgeFill());
        g.fillRoundedRectangle(badge, 3.0f);
        g.setColour(accent.withAlpha(0.7f));
        g.drawRoundedRectangle(badge, 3.0f, 0.8f);

        g.setColour(accent.brighter(0.3f));
        g.setFont(juce::Font(juce::jlimit(8.0f, 10.0f, bH * 0.75f), juce::Font::bold));
        g.drawFittedText(badgeText, badge.toNearestInt(), juce::Justification::centred, 1, 0.9f);
    }

    g.setColour(rackBorder().withAlpha(0.6f));
    g.drawRoundedRectangle(panelRect, corner, 1.0f);

    if (isBypassed && ! isMissing && ! isPlaceholder)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(panelRect, corner);

        g.setColour(rackText().withAlpha(0.85f));
        g.setFont(juce::Font(juce::jlimit(11.0f, 16.0f, panelRect.getHeight() * 0.28f), juce::Font::bold));
        g.drawFittedText("BYPASSED", area, juce::Justification::centred, 1, 0.9f);
    }
}

} // namespace forge7
