#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace juce
{
class AudioProcessorEditor;
class PluginDescription;
}

namespace forge7
{

namespace detail
{
class ClipPanMouseForwarder;
class PanInterceptLayer;
}

/** Hosts a native `AudioProcessorEditor` inside a clipped region above FORGE chrome.

    IMPORTANT DESIGN NOTE (embedded/pedal UX):

    VST plugin editors may or may not support resizing/scaling.
    - Some editors are fixed-size.
    - Some expose host-resizable bounds (`AudioProcessorEditor::isResizable()` + constrainer).
    - Some have internal scaling controls (inside the plugin UI).

    Therefore FORGE7 cannot depend on true plugin scaling.

    Strategy:
    1) Create the editor at its preferred/natural size (source of truth).
    2) Attempt host resize only when supported/safe (resizable editors).
    3) Wrap the editor in a clipped viewport/pan surface.
    4) Provide view modes + pan controls for 7-inch touchscreen usability.

    This component implements (2)-(4) while keeping the editor embedded (no OS window). */
class PluginEditorCanvas final : public juce::Component
{
public:
    enum class PluginEditorViewMode
    {
        ActualSize,
        FitToScreen,
        FitWidth,
    };

    /** Higher-level sizing intent applied when the hosted editor is attached or the viewport changes.

        Conservative by default: legacy editors and editors that do not declare
        `AudioProcessorEditor::isResizable()` are never stretched. */
    enum class PluginEditorSizingMode
    {
        /** Use the editor's natural / preferred size and centre it inside the viewport.

            Pan/scroll engages when the editor is larger than the viewport. **Never resizes the editor.** */
        NativeSizeCentered,

        /** Resize the editor to the viewport only when `isResizable()` is true, applying
            the editor's `getConstrainer()` if one is provided. Otherwise behaves like `NativeSizeCentered`. */
        FitIfResizable,

        /** Force resize to the viewport regardless of `isResizable()`. **Off by default**;
            intended only for explicitly known-safe editors (e.g. bench testing). Vendor GUIs
            that draw via native subviews / OpenGL can break in this mode. */
        FillViewportForKnownSafeEditors,
    };

    PluginEditorCanvas();
    ~PluginEditorCanvas() override;

    /** Non-owning; editor must outlive the canvas (parent owns `unique_ptr`). */
    void setHostedEditor(juce::AudioProcessorEditor* editor);

    /** Removes hosted editor from component hierarchy without deleting it. */
    void clearHostedEditor() noexcept;

    void setViewMode(PluginEditorViewMode mode);
    PluginEditorViewMode getViewMode() const noexcept { return viewMode; }

    /** Sets the policy used the next time the editor is attached or the viewport changes.

        Default is `NativeSizeCentered`. Internally maps to a compatible `PluginEditorViewMode`. */
    void setSizingMode(PluginEditorSizingMode mode);
    PluginEditorSizingMode getSizingMode() const noexcept { return sizingMode; }

    /** Optional format hint that tightens sizing decisions for legacy plugins.

        Stored for diagnostics + sizing safeguards (e.g. VST2 / "VST" stays `NativeSizeCentered`
        unless the editor itself is resizable). Pass before / immediately after `setHostedEditor`. */
    void setPluginDescriptionForSizing(const juce::PluginDescription& description);

    juce::String getPluginFormatNameForDiagnostics() const noexcept { return pluginFormatNameForDiagnostics; }
    juce::String getPluginDisplayNameForDiagnostics() const noexcept { return pluginDisplayNameForDiagnostics; }

    /** True when the resolved sizing policy considers `setSize` on the hosted editor safe. */
    bool isResizableUnderCurrentPolicy() const noexcept;

    /** Reset to ActualSize (the V1 default for fixed-size vendor GUIs); centers small editors,
        positions oversized editors at the top-left so scrollbars start at 0/0 like a web page. */
    void resetPluginViewToActualSize();

    void panBy(float deltaX, float deltaY);
    void setPanPosition(float x, float y);

    float getPanX() const noexcept { return panX; }
    float getPanY() const noexcept { return panY; }

    void toggleEncoderPanAxis() noexcept;

    bool getEncoderPanVertical() const noexcept { return encoderPanVertical; }

    void panWithEncoderDetents(int deltaSteps);

    bool canPanHorizontally() const noexcept;
    bool canPanVertically() const noexcept;

    /** Browser-like scroll wrappers. Scroll 0 = leftmost/topmost content, 1 = rightmost/bottommost. */
    bool canScrollX() const noexcept { return canPanHorizontally(); }
    bool canScrollY() const noexcept { return canPanVertically(); }

    float getScrollX01() const noexcept;
    float getScrollY01() const noexcept;

    void setScrollX01(float x);
    void setScrollY01(float y);

    void setPanMode(bool enabled);
    bool getPanMode() const noexcept { return panMode; }

    /** For embedding UI: min/max pan range in pixels (max is typically 0). */
    void getPanRangeX(float& minXOut, float& maxXOut) const noexcept;
    void getPanRangeY(float& minYOut, float& maxYOut) const noexcept;

    int getNaturalEditorWidth() const noexcept { return naturalW; }
    int getNaturalEditorHeight() const noexcept { return naturalH; }
    int getCurrentEditorWidth() const noexcept { return currentW; }
    int getCurrentEditorHeight() const noexcept { return currentH; }

    juce::Rectangle<int> getViewportBoundsForContent() const noexcept { return getLocalBounds(); }

    /** Bounds of the panned editor surface in canvas coordinates (empty if no editor). */
    juce::Rectangle<int> getHostedEditorBoundsInCanvas() const noexcept;

    /** True when the hosted editor is fixed-size and larger than the viewport (likely a native subview that may not clip). */
    bool hostedEditorMayExceedClipping() const noexcept;

    /** Detach and re-add the hosted editor to `panBoard` after viewport layout (helps some native peers). */
    void reattachHostedEditorIfPresent();

    /** Clip / panBoard / editor bounds and parent chain for logging. */
    juce::String describeHostedEditorLayoutForDiagnostics() const;

    void applyLayout();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    /** Used by `ClipPanMouseForwarder` for Alt/middle pan from clip + plugin subtree. */
    void forwardClipMouseDown(const juce::MouseEvent& e);
    void forwardClipMouseDrag(const juce::MouseEvent& e);
    void forwardClipMouseUp(const juce::MouseEvent& e);

private:
    void captureNaturalSizeFromEditor();
    void applyViewModeToEditorSize();
    void clampPan();
    void layoutPanBoardAndEditor();
    void updatePanInterceptLayer();

    bool hostedEditorIsResizable() const noexcept;

    juce::AudioProcessorEditor* hostedEditor = nullptr;

    juce::Component pluginContentClip;
    juce::Component panBoard;

    std::unique_ptr<detail::ClipPanMouseForwarder> clipForwarder;
    std::unique_ptr<detail::PanInterceptLayer> panIntercept;

    int naturalW { 800 };
    int naturalH { 500 };

    int currentW { 800 };
    int currentH { 500 };

    PluginEditorViewMode viewMode { PluginEditorViewMode::ActualSize };
    PluginEditorSizingMode sizingMode { PluginEditorSizingMode::NativeSizeCentered };

    /** Format name (e.g. "VST3", "VST", "AudioUnit") cached for sizing safeguards + DBG. */
    juce::String pluginFormatNameForDiagnostics;
    juce::String pluginDisplayNameForDiagnostics;

    /** Whitelisted format hint allows the FillViewportForKnownSafeEditors mode to actually fill;
        otherwise we downgrade to FitIfResizable for safety on unknown/legacy plugins. */
    bool descriptionAllowsForcedFill { false };

    float panX { 0.0f };
    float panY { 0.0f };

    bool encoderPanVertical { false };

    bool panMode { false };

    bool draggingPan { false };
    juce::Point<float> lastDragPos {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorCanvas)
};

} // namespace forge7
