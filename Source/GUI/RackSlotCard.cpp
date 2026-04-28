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

void RackSlotCard::refreshFromSlotInfo(const SlotInfo& info)
{
    lastInfo = info;
    bypassButton.setToggleState(info.bypass, juce::dontSendNotification);
    syncLabelsFromInfo();
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

    removeButton.setVisible(! lastInfo.isEmpty || lastInfo.missingPlugin || lastInfo.isPlaceholder);

    bypassButton.setVisible(! lastInfo.isEmpty || lastInfo.isPlaceholder || lastInfo.missingPlugin);
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
}

void RackSlotCard::resized()
{
    auto r = getLocalBounds().reduced(8);

    titleLabel.setBounds(r.removeFromTop(22));
    statusLabel.setBounds(r.removeFromTop(18));

    auto btnRow = r.removeFromBottom(36);
    bypassButton.setBounds(btnRow.removeFromLeft(juce::jmax(100, btnRow.getWidth() / 2)).reduced(0, 2));
    removeButton.setBounds(btnRow.reduced(4, 2));
}

void RackSlotCard::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (onSelect != nullptr)
        onSelect(slotIndex);
}

} // namespace forge7
