#include "PluginEditorCanvas.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace forge7
{
namespace
{
constexpr float kScaleMin = 0.35f;
constexpr float kScaleMax = 1.25f;
constexpr float kFallbackW = 800.0f;
constexpr float kFallbackH = 500.0f;

juce::Colour canvasBg() noexcept { return juce::Colour(0xff12161c); }
juce::Colour hudText() noexcept { return juce::Colour(0xffa8aeb8); }

float clampScale(float s) noexcept
{
    return juce::jlimit(kScaleMin, kScaleMax, s);
}

juce::String viewModeLabel(const PluginEditorCanvas::ViewMode m)
{
    switch (m)
    {
        case PluginEditorCanvas::ViewMode::FitHeight:
            return "Fit Height";
        case PluginEditorCanvas::ViewMode::FitWidth:
            return "Fit Width";
        case PluginEditorCanvas::ViewMode::FitAll:
            return "Fit All";
        case PluginEditorCanvas::ViewMode::Actual100:
            return "100%";
        default:
            return {};
    }
}

} // namespace

namespace detail
{

/** Forwards Alt/middle-drag pan from the clip layer (margins + passes-through when added recursively). */
class ClipPanMouseForwarder final : public juce::MouseListener
{
public:
    explicit ClipPanMouseForwarder(PluginEditorCanvas& ownerRef) noexcept : owner(ownerRef) {}

private:
    PluginEditorCanvas& owner;

    void mouseDown(const juce::MouseEvent& e) override { owner.forwardClipMouseDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override { owner.forwardClipMouseDrag(e); }
    void mouseUp(const juce::MouseEvent& e) override { owner.forwardClipMouseUp(e); }
};

} // namespace detail

PluginEditorCanvas::PluginEditorCanvas()
    : clipForwarder(std::make_unique<detail::ClipPanMouseForwarder>(*this))
{
    setOpaque(true);

    addAndMakeVisible(pluginContentClip);
    pluginContentClip.setInterceptsMouseClicks(true, true);
    pluginContentClip.addMouseListener(clipForwarder.get(), true);

    pluginContentClip.addAndMakeVisible(panBoard);
    panBoard.setInterceptsMouseClicks(true, true);

    hudLabel.setFont(juce::Font(11.0f));
    hudLabel.setColour(juce::Label::textColourId, hudText());
    hudLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hudLabel);

    panHintLabel.setFont(juce::Font(11.0f));
    panHintLabel.setColour(juce::Label::textColourId, hudText().withAlpha(0.85f));
    panHintLabel.setJustificationType(juce::Justification::centredRight);
    panHintLabel.setText("Alt/middle-drag to pan", juce::dontSendNotification);
    addAndMakeVisible(panHintLabel);
}

PluginEditorCanvas::~PluginEditorCanvas()
{
    pluginContentClip.removeMouseListener(clipForwarder.get());
    clearHostedEditor();
}

void PluginEditorCanvas::setHostedEditor(juce::AudioProcessorEditor* editor)
{
    clearHostedEditor();
    hostedEditor = editor;

    if (hostedEditor == nullptr)
        return;

    panBoard.addAndMakeVisible(*hostedEditor);

    hudLabel.toFront(false);
    panHintLabel.toFront(false);

    captureNativeSizeFromEditor();
    applyLayout();
}

void PluginEditorCanvas::clearHostedEditor() noexcept
{
    if (hostedEditor != nullptr)
    {
        panBoard.removeChildComponent(hostedEditor);
        hostedEditor = nullptr;
    }
}

void PluginEditorCanvas::setViewMode(const ViewMode mode)
{
    viewMode = mode;
    panX = panY = 0.0f;
    applyLayout();
}

void PluginEditorCanvas::captureNativeSizeFromEditor()
{
    if (hostedEditor == nullptr)
        return;

    int w = hostedEditor->getWidth();
    int h = hostedEditor->getHeight();

    if (w < 50 || h < 50 || w > 8000 || h > 8000)
    {
        w = static_cast<int>(kFallbackW);
        h = static_cast<int>(kFallbackH);
    }

    nativeW = w;
    nativeH = h;

    layoutPanBoardAndEditor();
}

int PluginEditorCanvas::scaledEditorWidth() const noexcept
{
    return juce::jmax(1, juce::roundToInt(static_cast<float>(nativeW) * scale));
}

int PluginEditorCanvas::scaledEditorHeight() const noexcept
{
    return juce::jmax(1, juce::roundToInt(static_cast<float>(nativeH) * scale));
}

void PluginEditorCanvas::layoutPanBoardAndEditor()
{
    if (hostedEditor == nullptr)
        return;

    const int sw = scaledEditorWidth();
    const int sh = scaledEditorHeight();

    hostedEditor->setBounds(0, 0, sw, sh);

    panBoard.setBounds(juce::roundToInt(panX),
                       juce::roundToInt(panY),
                       sw,
                       sh);
}

void PluginEditorCanvas::applyLayout()
{
    recomputeScaleAndPan();
    layoutPanBoardAndEditor();

    hudLabel.setText(getViewHudLine(), juce::dontSendNotification);

    const bool showPanHint = canPanHorizontally() || canPanVertically();
    panHintLabel.setVisible(showPanHint);
    panHintLabel.setText(showPanHint ? "Drag to pan (Alt/middle)" : "", juce::dontSendNotification);

    repaint();
}

bool PluginEditorCanvas::canPanHorizontally() const noexcept
{
    auto content = getLocalBounds();

    if (content.getHeight() <= kHudStripHeight)
        return false;

    content.removeFromBottom(kHudStripHeight);
    const float cw = static_cast<float>(content.getWidth());
    const float scaledW = static_cast<float>(scaledEditorWidth());
    return scaledW > cw + 0.5f;
}

bool PluginEditorCanvas::canPanVertically() const noexcept
{
    auto content = getLocalBounds();

    if (content.getHeight() <= kHudStripHeight)
        return false;

    content.removeFromBottom(kHudStripHeight);
    const float ch = static_cast<float>(content.getHeight());
    const float scaledH = static_cast<float>(scaledEditorHeight());
    return scaledH > ch + 0.5f;
}

void PluginEditorCanvas::panWithEncoderDetents(const int deltaSteps)
{
    const float step = 32.0f * static_cast<float>(deltaSteps);

    if (encoderPanVertical)
    {
        if (canPanVertically())
            panBy(0.0f, step);
        else if (canPanHorizontally())
            panBy(step, 0.0f);
    }
    else
    {
        if (canPanHorizontally())
            panBy(step, 0.0f);
        else if (canPanVertically())
            panBy(0.0f, step);
    }
}

void PluginEditorCanvas::recomputeScaleAndPan()
{
    auto content = getLocalBounds();

    if (content.getHeight() <= kHudStripHeight + 4 || content.getWidth() < 4)
        return;

    content.removeFromBottom(kHudStripHeight);

    const float cw = juce::jmax(1.0f, static_cast<float>(content.getWidth()));
    const float ch = juce::jmax(1.0f, static_cast<float>(content.getHeight()));
    const float nw = static_cast<float>(nativeW);
    const float nh = static_cast<float>(nativeH);

    switch (viewMode)
    {
        case ViewMode::FitHeight:
            scale = clampScale(ch / nh);
            break;
        case ViewMode::FitWidth:
            scale = clampScale(cw / nw);
            break;
        case ViewMode::FitAll:
            scale = clampScale(juce::jmin(cw / nw, ch / nh));
            break;
        case ViewMode::Actual100:
            scale = 1.0f;
            break;
    }

    clampPan();
}

void PluginEditorCanvas::clampPan()
{
    auto content = getLocalBounds();

    if (content.getHeight() > kHudStripHeight)
        content.removeFromBottom(kHudStripHeight);

    const float cw = static_cast<float>(content.getWidth());
    const float ch = static_cast<float>(content.getHeight());
    const float scaledW = static_cast<float>(scaledEditorWidth());
    const float scaledH = static_cast<float>(scaledEditorHeight());

    if (scaledW <= cw)
        panX = (cw - scaledW) * 0.5f;
    else
        panX = juce::jlimit(cw - scaledW, 0.0f, panX);

    if (scaledH <= ch)
        panY = (ch - scaledH) * 0.5f;
    else
        panY = juce::jlimit(ch - scaledH, 0.0f, panY);
}

void PluginEditorCanvas::panBy(const float deltaX, const float deltaY)
{
    panX += deltaX;
    panY += deltaY;
    clampPan();
    layoutPanBoardAndEditor();

    hudLabel.setText(getViewHudLine(), juce::dontSendNotification);
    repaint();
}

void PluginEditorCanvas::toggleEncoderPanAxis() noexcept
{
    encoderPanVertical = !encoderPanVertical;
}

juce::String PluginEditorCanvas::getViewHudLine() const
{
    const int pct = juce::jlimit(0, 999, juce::roundToInt(scale * 100.0f));
    juce::String line = "View: " + viewModeLabel(viewMode) + " · Scale " + juce::String(pct) + "%";

    if (canPanHorizontally() || canPanVertically())
        line += " · Pan";

    return line;
}

void PluginEditorCanvas::paint(juce::Graphics& g)
{
    g.fillAll(canvasBg());
}

void PluginEditorCanvas::resized()
{
    auto r = getLocalBounds();
    auto hudRow = r.removeFromBottom(kHudStripHeight);
    hudLabel.setBounds(hudRow.removeFromLeft(juce::jmax(160, hudRow.getWidth() / 2)).reduced(6, 2));
    panHintLabel.setBounds(hudRow.reduced(6, 2));

    pluginContentClip.setBounds(r);

    applyLayout();

    hudLabel.toFront(false);
    panHintLabel.toFront(false);
}

void PluginEditorCanvas::mouseDown(const juce::MouseEvent& e)
{
    if (e.eventComponent == this && (e.mods.isMiddleButtonDown() || e.mods.isAltDown()))
    {
        draggingPan = true;
        lastDragPos = e.position;
    }
}

void PluginEditorCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (!draggingPan || e.eventComponent != this)
        return;

    const juce::Point<float> delta(e.position.x - lastDragPos.x, e.position.y - lastDragPos.y);
    lastDragPos = e.position;
    panBy(delta.x, delta.y);
}

void PluginEditorCanvas::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (e.eventComponent == this)
        draggingPan = false;
}

void PluginEditorCanvas::forwardClipMouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        draggingPan = true;
        lastDragPos = e.position;
    }
}

void PluginEditorCanvas::forwardClipMouseDrag(const juce::MouseEvent& e)
{
    if (!draggingPan)
        return;

    const juce::Point<float> delta(e.position.x - lastDragPos.x, e.position.y - lastDragPos.y);
    lastDragPos = e.position;
    panBy(delta.x, delta.y);
}

void PluginEditorCanvas::forwardClipMouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    draggingPan = false;
}

} // namespace forge7
