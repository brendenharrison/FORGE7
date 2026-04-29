#pragma once

#include <array>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginHost/PluginChain.h"
#include "CpuMeter.h"
#include "VuMeterComponent.h"
#include "RackSlotCard.h"
#include "UiTextAsciiPolicy.h"

namespace forge7
{

struct AppContext;
class PluginBrowserComponent;
class ChainControlsPanelComponent;

/** Edit Mode ("Rack"): status bar, chain lane, always-visible control assignments strip, slot context strip,

    fullscreen in-app plugin browser. Touch-first ~7". Message thread only. */
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

    /** Overlay hidden; rollback pending add state only when `cancelled` is true. */
    void dismissPluginBrowserOverlay(bool cancelled);

    /** Loads into the resolved target slot for the active browser session. Returns false if load failed. */
    bool loadPluginIntoSelectedSlot(const juce::PluginDescription& desc);

    int resolveTargetSlotForPluginLoad() const noexcept;
    int findFirstEmptyBackendSlotIndex() const noexcept;

    /** Move plugins left to eliminate leading-empty gaps while keeping contiguous order. Message thread only. */
    void compactChainLeadingGapsFromPluginLoads();

    void promptAddChain();
    void promptRenameActiveChain();
    void promptAddScene();
    void promptRenameActiveScene();

    std::vector<int> getVisiblePluginSlotIndices() const;

    [[nodiscard]] static bool rackSlotShowsContent(const SlotInfo& info) noexcept;

    [[nodiscard]] static bool chainSlotShowsInRailLine(int pendingAddSlotIdx, int slotIdx, const SlotInfo& info) noexcept;

    enum class BrowserOpenReason
    {
        None,
        AddNewSlot,
        ReplaceExistingSlot
    };

    AppContext& appContext;

    int selectedSlotIndex { -1 };
    int pendingAddSlotIndex { -1 };
    int selectionBeforePendingAdd { -1 };
    BrowserOpenReason browserOpenReason { BrowserOpenReason::None };

    juce::Label projectHeaderLabel;
    juce::Label projectDirtyLabel;
    juce::Label sceneTitleLabel;
    juce::Label chainHeaderLabel;
    juce::Label editModeBadge;
    juce::Label tempoLabel;
    juce::Label chainCountLabel;
    juce::TextButton chainPrevButton { "Chain -" };
    juce::TextButton chainNextButton { "Chain +" };
    juce::TextButton addChainButton { "Add Chain" };
    juce::TextButton renameChainButton { "Rename Chain" };
    juce::TextButton addSceneButton { "Add Scene" };
    juce::TextButton renameSceneButton { "Rename Scene" };
    std::unique_ptr<CpuMeter> cpuMeter;
    juce::ToggleButton globalBypassFxToggle { "Bypass FX" };

    juce::Viewport chainViewport; // Center lane only (plugins + add card). IO blocks are fixed outside.
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

    /** Peak meters: chain input (pre-plugin), post-slot (after each slot in chain lane), final output. */
    std::unique_ptr<VuMeterComponent> rackInputVuMeter;
    std::unique_ptr<VuMeterComponent> rackOutputVuMeter;
    std::array<std::unique_ptr<VuMeterComponent>, kPluginChainMaxSlots> rackPostSlotVuMeters {};
    std::array<std::unique_ptr<juce::Label>, 9> arrowLabels {};
    std::array<std::unique_ptr<RackSlotCard>, kPluginChainMaxSlots> slotCards {};

    class AddPluginCard final : public juce::Component
    {
    public:
        std::function<void()> onAddClicked;
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
    };

    std::unique_ptr<AddPluginCard> addPluginCard;

    juce::TextButton ctxMoveLeftButton { "<" };
    juce::TextButton ctxMoveRightButton { ">" };
    juce::ToggleButton ctxBypassToggle { "Bypass" };
    juce::TextButton ctxRemoveButton { "Remove" };
    juce::TextButton ctxReplaceButton { "Replace" };
    juce::TextButton ctxEditorButton { "Editor" };

    juce::TextButton navPerformanceButton;
    juce::TextButton settingsButton { "Settings" };

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    juce::TextButton simHwButton { "Sim HW" };
#endif

    std::unique_ptr<juce::Component> browserOverlay;
    std::unique_ptr<PluginBrowserComponent> pluginBrowser;

    std::unique_ptr<ChainControlsPanelComponent> chainControlsPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RackViewComponent)
};

} // namespace forge7
