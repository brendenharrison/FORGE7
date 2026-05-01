#include "PluginEditorCanvas.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace forge7
{
namespace
{
constexpr float kFallbackW = 400.0f;
constexpr float kFallbackH = 300.0f;

juce::Colour canvasBg() noexcept { return juce::Colour(0xff12161c); }
juce::Colour hudText() noexcept { return juce::Colour(0xffa8aeb8); }

juce::String viewModeLabel(const PluginEditorCanvas::PluginEditorViewMode m)
{
    switch (m)
    {
        case PluginEditorCanvas::PluginEditorViewMode::ActualSize:
            return "Actual";
        case PluginEditorCanvas::PluginEditorViewMode::FitWidth:
            return "Fit Width";
        case PluginEditorCanvas::PluginEditorViewMode::FitToScreen:
            return "Fit";
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

/** When Pan Mode is enabled, this transparent layer captures touch/mouse drags and pans the viewport,
    preventing accidental plugin control interaction. */
class PanInterceptLayer final : public juce::Component
{
public:
    explicit PanInterceptLayer(PluginEditorCanvas& ownerRef) noexcept : owner(ownerRef)
    {
        setInterceptsMouseClicks(true, true);
    }

    void paint(juce::Graphics& g) override { juce::ignoreUnused(g); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        active = true;
        last = e.position;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!active)
            return;

        const juce::Point<float> d(e.position.x - last.x, e.position.y - last.y);
        last = e.position;
        owner.panBy(d.x, d.y);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        juce::ignoreUnused(e);
        active = false;
    }

private:
    PluginEditorCanvas& owner;
    bool active { false };
    juce::Point<float> last {};
};

} // namespace detail

PluginEditorCanvas::PluginEditorCanvas()
    : clipForwarder(std::make_unique<detail::ClipPanMouseForwarder>(*this))
{
    setOpaque(true);
    setPaintingIsUnclipped(false);

    addAndMakeVisible(pluginContentClip);
    pluginContentClip.setInterceptsMouseClicks(true, true);
    pluginContentClip.setPaintingIsUnclipped(false);
    pluginContentClip.addMouseListener(clipForwarder.get(), true);

    pluginContentClip.addAndMakeVisible(panBoard);
    panBoard.setInterceptsMouseClicks(true, true);
    panBoard.setPaintingIsUnclipped(false);

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

    captureNaturalSizeFromEditor();
    applyLayout();
}

void PluginEditorCanvas::clearHostedEditor() noexcept
{
    if (hostedEditor != nullptr)
    {
        panBoard.removeChildComponent(hostedEditor);
        hostedEditor = nullptr;
    }

    panIntercept.reset();
}

void PluginEditorCanvas::setViewMode(const PluginEditorViewMode mode)
{
    viewMode = mode;
    panX = panY = 0.0f;
    applyLayout();
}

void PluginEditorCanvas::setPanMode(const bool enabled)
{
    if (panMode == enabled)
        return;

    panMode = enabled;
    updatePanInterceptLayer();
    repaint();
}

void PluginEditorCanvas::captureNaturalSizeFromEditor()
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

    naturalW = w;
    naturalH = h;
    currentW = w;
    currentH = h;

    layoutPanBoardAndEditor();
}

int PluginEditorCanvas::viewportWidth() const noexcept
{
    auto r = getLocalBounds();
    if (r.getHeight() <= kHudStripHeight)
        return 0;
    r.removeFromBottom(kHudStripHeight);
    return r.getWidth();
}

int PluginEditorCanvas::viewportHeight() const noexcept
{
    auto r = getLocalBounds();
    if (r.getHeight() <= kHudStripHeight)
        return 0;
    r.removeFromBottom(kHudStripHeight);
    return r.getHeight();
}

juce::Rectangle<int> PluginEditorCanvas::getViewportBoundsForContent() const noexcept
{
    auto r = getLocalBounds();
    if (r.getHeight() <= kHudStripHeight)
        return {};
    r.removeFromBottom(kHudStripHeight);
    return r;
}

juce::Rectangle<int> PluginEditorCanvas::getHostedEditorBoundsInCanvas() const noexcept
{
    if (hostedEditor == nullptr)
        return {};

    const auto panInClip = panBoard.getBounds();
    const auto clipInCanvas = pluginContentClip.getBounds();
    return panInClip.withPosition(clipInCanvas.getX() + panInClip.getX(), clipInCanvas.getY() + panInClip.getY());
}

bool PluginEditorCanvas::hostedEditorIsResizable() const noexcept
{
    return hostedEditor != nullptr && hostedEditor->isResizable();
}

void PluginEditorCanvas::layoutPanBoardAndEditor()
{
    if (hostedEditor == nullptr)
        return;

    hostedEditor->setBounds(0, 0, currentW, currentH);

    panBoard.setBounds(juce::roundToInt(panX),
                       juce::roundToInt(panY),
                       currentW,
                       currentH);
}

void PluginEditorCanvas::applyLayout()
{
    applyViewModeToEditorSize();
    layoutPanBoardAndEditor();
    updatePanInterceptLayer();

    hudLabel.setText(getViewHudLine(), juce::dontSendNotification);

    const bool showPanHint = canPanHorizontally() || canPanVertically();
    panHintLabel.setVisible(showPanHint);
    if (panMode)
        panHintLabel.setText("Pan mode: drag to pan", juce::dontSendNotification);
    else
        panHintLabel.setText(showPanHint ? "Alt/middle-drag to pan" : "", juce::dontSendNotification);

    repaint();
}

bool PluginEditorCanvas::canPanHorizontally() const noexcept
{
    const auto content = getViewportBoundsForContent();
    return currentW > content.getWidth() + 1;
}

bool PluginEditorCanvas::canPanVertically() const noexcept
{
    const auto content = getViewportBoundsForContent();
    return currentH > content.getHeight() + 1;
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

void PluginEditorCanvas::applyViewModeToEditorSize()
{
    if (hostedEditor == nullptr)
        return;

    const auto content = getViewportBoundsForContent();
    const int vw = juce::jmax(1, content.getWidth());
    const int vh = juce::jmax(1, content.getHeight());

    // In V1, we never apply affine transforms to native plugin editors. We only attempt to resize
    // editors that explicitly support host resizing; otherwise we keep natural size and rely on panning.
    if (viewMode == PluginEditorViewMode::ActualSize)
    {
        currentW = naturalW;
        currentH = naturalH;
        hostedEditor->setSize(currentW, currentH);
        currentW = hostedEditor->getWidth();
        currentH = hostedEditor->getHeight();
    }
    else if (viewMode == PluginEditorViewMode::FitToScreen)
    {
        if (hostedEditorIsResizable())
        {
            hostedEditor->setSize(vw, vh);
            currentW = hostedEditor->getWidth();
            currentH = hostedEditor->getHeight();
        }
        else
        {
            currentW = naturalW;
            currentH = naturalH;
        }
    }
    else if (viewMode == PluginEditorViewMode::FitWidth)
    {
        if (hostedEditorIsResizable())
        {
            // Keep aspect-ish by scaling height proportionally to natural size, but still allow constrainer.
            const float aspect = naturalH > 0 ? (static_cast<float>(naturalH) / static_cast<float>(naturalW)) : 0.75f;
            const int targetH = juce::jmax(1, juce::roundToInt(static_cast<float>(vw) * aspect));
            hostedEditor->setSize(vw, targetH);
            currentW = hostedEditor->getWidth();
            currentH = hostedEditor->getHeight();
        }
        else
        {
            currentW = naturalW;
            currentH = naturalH;
        }
    }

    clampPan();
}

void PluginEditorCanvas::clampPan()
{
    const auto content = getViewportBoundsForContent();
    const float cw = static_cast<float>(content.getWidth());
    const float ch = static_cast<float>(content.getHeight());
    const float w = static_cast<float>(currentW);
    const float h = static_cast<float>(currentH);

    if (w <= cw)
        panX = (cw - w) * 0.5f;
    else
        panX = juce::jlimit(cw - w, 0.0f, panX);

    if (h <= ch)
        panY = (ch - h) * 0.5f;
    else
        panY = juce::jlimit(ch - h, 0.0f, panY);
}

void PluginEditorCanvas::getPanRangeX(float& minXOut, float& maxXOut) const noexcept
{
    const auto content = getViewportBoundsForContent();
    const float cw = static_cast<float>(content.getWidth());
    const float w = static_cast<float>(currentW);

    if (w <= cw)
    {
        const float centred = (cw - w) * 0.5f;
        minXOut = centred;
        maxXOut = centred;
        return;
    }

    minXOut = cw - w;
    maxXOut = 0.0f;
}

void PluginEditorCanvas::getPanRangeY(float& minYOut, float& maxYOut) const noexcept
{
    const auto content = getViewportBoundsForContent();
    const float ch = static_cast<float>(content.getHeight());
    const float h = static_cast<float>(currentH);

    if (h <= ch)
    {
        const float centred = (ch - h) * 0.5f;
        minYOut = centred;
        maxYOut = centred;
        return;
    }

    minYOut = ch - h;
    maxYOut = 0.0f;
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

void PluginEditorCanvas::setPanPosition(const float x, const float y)
{
    panX = x;
    panY = y;
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
    const auto vp = getViewportBoundsForContent();
    const juce::String sizeLine = juce::String(currentW) + "x" + juce::String(currentH);
    const juce::String vpLine = juce::String(vp.getWidth()) + "x" + juce::String(vp.getHeight());

    juce::String line = viewModeLabel(viewMode) + " | Editor " + sizeLine + " | View " + vpLine;

    if (!hostedEditorIsResizable() && viewMode != PluginEditorViewMode::ActualSize)
        line += " | Fixed-size";

    if (panMode)
        line += " | Pan mode";
    else if (canPanHorizontally() || canPanVertically())
        line += " | Pan";

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

void PluginEditorCanvas::updatePanInterceptLayer()
{
    if (panMode)
    {
        if (panIntercept == nullptr)
        {
            panIntercept = std::make_unique<detail::PanInterceptLayer>(*this);
            pluginContentClip.addAndMakeVisible(*panIntercept);
        }

        panIntercept->setBounds(pluginContentClip.getLocalBounds());
        panIntercept->toFront(false);
    }
    else if (panIntercept != nullptr)
    {
        pluginContentClip.removeChildComponent(panIntercept.get());
        panIntercept.reset();
    }
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
