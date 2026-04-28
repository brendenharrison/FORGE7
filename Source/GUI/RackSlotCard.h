#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginHost/PluginChain.h"

namespace forge7
{

/** One touch-friendly slot in the horizontal rack (Edit Mode).

    Threading: message thread only - host queries from `PluginChain::getSlotInfo`. */
class RackSlotCard final : public juce::Component
{
public:
    RackSlotCard();
    ~RackSlotCard() override;

    void setSlotIndex(int index) noexcept;
    int getSlotIndex() const noexcept { return slotIndex; }

    void setSelected(bool selected) noexcept;
    /** When false, bypass/remove are hidden; use rack context strip for actions (embedded UI). */
    void setShowInlineControls(bool show) noexcept;

    void refreshFromSlotInfo(const SlotInfo& info);

    std::function<void(int slotIndex)> onSelect;
    std::function<void(int slotIndex, bool bypassed)> onBypassChanged;
    std::function<void(int slotIndex)> onRemoveRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    int slotIndex { 0 };
    bool selected { false };

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::ToggleButton bypassButton { "Bypass" };
    juce::TextButton removeButton { "Remove" };

    SlotInfo lastInfo {};
    bool showInlineControls { true };

    void syncLabelsFromInfo();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RackSlotCard)
};

} // namespace forge7
