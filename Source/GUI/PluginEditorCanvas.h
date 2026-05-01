#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace juce
{
class AudioProcessorEditor;
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

    PluginEditorCanvas();
    ~PluginEditorCanvas() override;

    /** Non-owning; editor must outlive the canvas (parent owns `unique_ptr`). */
    void setHostedEditor(juce::AudioProcessorEditor* editor);

    /** Removes hosted editor from component hierarchy without deleting it. */
    void clearHostedEditor() noexcept;

    void setViewMode(PluginEditorViewMode mode);
    PluginEditorViewMode getViewMode() const noexcept { return viewMode; }

    void panBy(float deltaX, float deltaY);
    void setPanPosition(float x, float y);

    float getPanX() const noexcept { return panX; }
    float getPanY() const noexcept { return panY; }

    void toggleEncoderPanAxis() noexcept;

    bool getEncoderPanVertical() const noexcept { return encoderPanVertical; }

    void panWithEncoderDetents(int deltaSteps);

    bool canPanHorizontally() const noexcept;
    bool canPanVertically() const noexcept;

    void setPanMode(bool enabled);
    bool getPanMode() const noexcept { return panMode; }

    /** For embedding UI: min/max pan range in pixels (max is typically 0). */
    void getPanRangeX(float& minXOut, float& maxXOut) const noexcept;
    void getPanRangeY(float& minYOut, float& maxYOut) const noexcept;

    int getNaturalEditorWidth() const noexcept { return naturalW; }
    int getNaturalEditorHeight() const noexcept { return naturalH; }
    int getCurrentEditorWidth() const noexcept { return currentW; }
    int getCurrentEditorHeight() const noexcept { return currentH; }

    juce::Rectangle<int> getViewportBoundsForContent() const noexcept;

    /** Bounds of the panned editor surface in canvas coordinates (empty if no editor). */
    juce::Rectangle<int> getHostedEditorBoundsInCanvas() const noexcept;

    /** Detach and re-add the hosted editor to `panBoard` after viewport layout (helps some native peers). */
    void reattachHostedEditorIfPresent();

    /** Clip / panBoard / editor bounds and parent chain for logging. */
    juce::String describeHostedEditorLayoutForDiagnostics() const;

    void applyLayout();

    juce::String getViewHudLine() const;

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

    int viewportWidth() const noexcept;
    int viewportHeight() const noexcept;
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

    PluginEditorViewMode viewMode { PluginEditorViewMode::FitToScreen };

    float panX { 0.0f };
    float panY { 0.0f };

    bool encoderPanVertical { false };

    bool panMode { false };

    bool draggingPan { false };
    juce::Point<float> lastDragPos {};

    juce::Label hudLabel;
    juce::Label panHintLabel;

    static constexpr int kHudStripHeight = 22;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorCanvas)
};

} // namespace forge7
