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
}

/** Hosts a native `AudioProcessorEditor` inside a clipped region above FORGE chrome.

    VST3 editors on macOS embed **native views** (NSView) that often **ignore** `Component` affine
    transforms. Scaling is applied by **resizing** the editor to scaled pixel dimensions and moving it
    inside `panBoard`, clipped by `pluginContentClip`. */
class PluginEditorCanvas final : public juce::Component
{
public:
    enum class ViewMode
    {
        FitHeight,
        FitWidth,
        FitAll,
        Actual100,
    };

    PluginEditorCanvas();
    ~PluginEditorCanvas() override;

    /** Non-owning; editor must outlive the canvas (parent owns `unique_ptr`). */
    void setHostedEditor(juce::AudioProcessorEditor* editor);

    /** Removes hosted editor from component hierarchy without deleting it. */
    void clearHostedEditor() noexcept;

    void setViewMode(ViewMode mode);
    ViewMode getViewMode() const noexcept { return viewMode; }

    void panBy(float deltaX, float deltaY);

    void toggleEncoderPanAxis() noexcept;

    bool getEncoderPanVertical() const noexcept { return encoderPanVertical; }

    void panWithEncoderDetents(int deltaSteps);

    bool canPanHorizontally() const noexcept;
    bool canPanVertically() const noexcept;

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
    void captureNativeSizeFromEditor();
    void recomputeScaleAndPan();
    void clampPan();
    void layoutPanBoardAndEditor();

    int scaledEditorWidth() const noexcept;
    int scaledEditorHeight() const noexcept;

    juce::AudioProcessorEditor* hostedEditor = nullptr;

    juce::Component pluginContentClip;
    juce::Component panBoard;

    std::unique_ptr<detail::ClipPanMouseForwarder> clipForwarder;

    int nativeW { 800 };
    int nativeH { 500 };

    ViewMode viewMode { ViewMode::FitHeight };

    float scale { 1.0f };
    float panX { 0.0f };
    float panY { 0.0f };

    bool encoderPanVertical { false };

    bool draggingPan { false };
    juce::Point<float> lastDragPos {};

    juce::Label hudLabel;
    juce::Label panHintLabel;

    static constexpr int kHudStripHeight = 22;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorCanvas)
};

} // namespace forge7
