#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginHost/PluginChain.h"
#include "CpuMeter.h"
#include "RackSlotCard.h"

namespace forge7
{

struct AppContext;
class PluginBrowserComponent;
class PluginInspectorComponent;

/** Edit Mode rack: status bar, horizontal scrollable chain (IN → slots → OUT), plugin browser overlay.

    Touch-first layout for ~7" panels (large tap targets, dark theme). Message thread only. */
class RackViewComponent final : public juce::Component,
                                private juce::Timer
{
public:
    explicit RackViewComponent(AppContext& context);
    ~RackViewComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void syncEncoderFocus();

    bool isPluginBrowserVisible() const noexcept;

    /** Encoder long-press escape / dismiss browser. */
    void closePluginBrowser();

    /** Dev tools / external callers: select a slot (1-based UI uses slot card tap). */
    void selectRackSlot(int slotIndex);

    int getSelectedSlotIndex() const noexcept { return selectedSlotIndex; }

    void showPluginBrowser();

    /** After `ProjectSerializer::loadProjectFromFile` hydrates the host chain. */
    void refreshAfterProjectHydration();

private:
    void timerCallback() override;

    void refreshSlotDisplays();
    void layoutRackChain();
    void wireSlotCallbacks(int slotIndex);
    void setSelectedSlot(int slotIndex);

    void hidePluginBrowser();

    /** Loads into the selected rack slot. Returns false if load failed (browser stays open + error shown). */
    bool loadPluginIntoSelectedSlot(const juce::PluginDescription& desc);

    AppContext& appContext;

    int selectedSlotIndex { -1 };

    juce::Label sceneTitleLabel;
    juce::Label variationLabel;
    juce::Label editModeBadge;
    juce::Label tempoLabel;
    std::unique_ptr<CpuMeter> cpuMeter;
    juce::TextButton menuButton { "Menu" };

    juce::Viewport chainViewport;
    std::unique_ptr<juce::Component> chainContent;

    class IoBlock final : public juce::Component
    {
    public:
        enum class Kind
        {
            Input,
            Output
        };

        explicit IoBlock(Kind k);

        void paint(juce::Graphics& g) override;

    private:
        Kind kind;
    };

    std::unique_ptr<IoBlock> inputBlock;
    std::unique_ptr<IoBlock> outputBlock;
    std::array<std::unique_ptr<juce::Label>, 9> arrowLabels {};
    std::array<std::unique_ptr<RackSlotCard>, kPluginChainMaxSlots> slotCards {};

    juce::TextButton addPluginButton { "Add Plugin" };

    juce::TextButton navPerformanceButton;
    juce::TextButton navScenesButton;
    juce::TextButton navMixButton;
    juce::ToggleButton bypassFxButton { "Bypass FX" };
    juce::TextButton settingsButton { "Settings" };

    std::unique_ptr<juce::Component> browserOverlay;
    std::unique_ptr<PluginBrowserComponent> pluginBrowser;

    std::unique_ptr<PluginInspectorComponent> pluginInspector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RackViewComponent)
};

} // namespace forge7
