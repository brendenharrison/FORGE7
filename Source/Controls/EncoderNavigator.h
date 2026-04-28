#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ControlManager.h"
#include "EncoderFocusTypes.h"
#include "HardwareControlTypes.h"

namespace forge7
{

struct AppContext;

/** Hardware encoder -> focus ring + activate / escape. Touch-agnostic: draws a highlight ring only.

    Typical wiring: screens build `std::vector<EncoderFocusItem>` and call `setRootFocusChain`.

    Modal UI (e.g. plugin browser) calls `setModalFocusChain` to replace navigation until `clearModalFocusChain`. */
class EncoderNavigator final : public juce::Component,
                               private ControlManager::Listener
{
public:
    EncoderNavigator();
    ~EncoderNavigator() override;

    void attachContext(AppContext* context) noexcept;

    /** Root chain for the current full-screen mode (performance vs rack). Preserves focus index when possible. */
    void setRootFocusChain(std::vector<EncoderFocusItem> items);

    /** Overlay chain (modal). Clears automatically when dismissed. Starts focus at first item. */
    void setModalFocusChain(std::vector<EncoderFocusItem> items);

    void clearModalFocusChain() noexcept;

    bool hasModalFocusChain() const noexcept { return modalActive; }

    /** Long-press encoder: close modal first, then optional app-level back (e.g. leave edit mode). */
    void setEscapeHandler(std::function<void()> handler) { escapeHandler = std::move(handler); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Overlay must not steal mouse/touch from Rack or Performance UI (see MainComponent stacking order). */
    bool hitTest(int x, int y) override;

    /** Dev / simulated hardware panel: short description of focus ring target. */
    juce::String getFocusDebugSummary() const;

private:
    void encoderNavigationEvent(const HardwareControlEvent& event) override;

    const std::vector<EncoderFocusItem>& activeChain() const noexcept;
    std::vector<EncoderFocusItem>& activeChain() noexcept;

    void moveFocusBySteps(int deltaSteps);
    void activateFocused();
    void repaintFocusArea();

    AppContext* appContext = nullptr;
    ControlManager* attachedControlManager = nullptr;

    std::vector<EncoderFocusItem> rootItems;
    std::vector<EncoderFocusItem> modalItems;
    bool modalActive = false;

    int focusIndex = 0;

    std::function<void()> escapeHandler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EncoderNavigator)
};

} // namespace forge7
