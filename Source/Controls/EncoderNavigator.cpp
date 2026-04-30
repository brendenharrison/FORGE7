#include "EncoderNavigator.h"

#include "../App/AppContext.h"
#include "../Utilities/Logger.h"
#include "ControlManager.h"

namespace forge7
{
namespace
{
constexpr float kFocusRingExpand = 4.0f;
constexpr float kFocusCornerRadius = 7.0f;
constexpr float kFocusStroke = 3.0f;

juce::Colour focusRingColour() noexcept
{
    return juce::Colour(0xff6bc4ff);
}
} // namespace

EncoderNavigator::EncoderNavigator()
{
    setInterceptsMouseClicks(false, false);
    setOpaque(false);
}

EncoderNavigator::~EncoderNavigator()
{
    if (attachedControlManager != nullptr)
        attachedControlManager->removeListener(this);
}

bool EncoderNavigator::hitTest(int x, int y)
{
    juce::ignoreUnused(x, y);
    return false;
}

void EncoderNavigator::attachContext(AppContext* context) noexcept
{
    if (attachedControlManager != nullptr)
    {
        attachedControlManager->removeListener(this);
        attachedControlManager = nullptr;
    }

    appContext = context;

    if (appContext != nullptr && appContext->controlManager != nullptr)
    {
        attachedControlManager = appContext->controlManager;
        attachedControlManager->addListener(this);
        Logger::info("FORGE7 EncoderNavigator: attached to ControlManager");
    }
    else if (appContext != nullptr && appContext->controlManager == nullptr)
    {
        Logger::error("FORGE7 EncoderNavigator: attachContext skipped - AppContext.controlManager is null");
    }
}

const std::vector<EncoderFocusItem>& EncoderNavigator::activeChain() const noexcept
{
    return modalActive ? modalItems : rootItems;
}

std::vector<EncoderFocusItem>& EncoderNavigator::activeChain() noexcept
{
    return modalActive ? modalItems : rootItems;
}

void EncoderNavigator::setRootFocusChain(std::vector<EncoderFocusItem> items)
{
    const int oldIdx = focusIndex;
    rootItems = std::move(items);

    if (!modalActive)
    {
        if (rootItems.empty())
            focusIndex = -1;
        else
            focusIndex = juce::jlimit(0, static_cast<int>(rootItems.size()) - 1, oldIdx);
    }

    if (!modalActive)
        triggerOnFocusForCurrentItem();

    repaint();
}

void EncoderNavigator::setModalFocusChain(std::vector<EncoderFocusItem> items)
{
    modalItems = std::move(items);
    modalActive = !modalItems.empty();
    focusIndex = modalActive ? 0 : juce::jlimit(-1, static_cast<int>(rootItems.size()) - 1, focusIndex);
    triggerOnFocusForCurrentItem();
    repaint();
}

void EncoderNavigator::clearModalFocusChain() noexcept
{
    if (!modalActive)
        return;

    modalActive = false;
    modalItems.clear();

    if (rootItems.empty())
        focusIndex = -1;
    else
        focusIndex = juce::jlimit(0, static_cast<int>(rootItems.size()) - 1, focusIndex);

    triggerOnFocusForCurrentItem();
    repaint();
}

void EncoderNavigator::clearRootFocusChain() noexcept
{
    rootItems.clear();

    if (!modalActive)
        focusIndex = -1;

    repaint();
}

void EncoderNavigator::clearAllFocus(const bool logIfCleared) noexcept
{
    modalActive = false;
    modalItems.clear();
    rootItems.clear();
    focusIndex = -1;
    repaint();

    if (logIfCleared)
        Logger::info("FORGE7 Focus: clearAllFocus");
}

void EncoderNavigator::setFocusOverlayEnabled(const bool shouldDraw) noexcept
{
    if (focusOverlayEnabled == shouldDraw)
        return;

    focusOverlayEnabled = shouldDraw;
    repaint();
}

void EncoderNavigator::encoderNavigationEvent(const HardwareControlEvent& event)
{
    if (event.id == HardwareControlId::EncoderRotate && event.type == HardwareControlType::RelativeDelta)
    {
        const int detents = juce::roundToInt(event.value);
        if (detents == 0)
            return;

        auto& chain = activeChain();
        if (chain.empty() || focusIndex < 0 || focusIndex >= static_cast<int>(chain.size()))
            return;

        auto& item = chain[static_cast<size_t>(focusIndex)];

        if (item.onRotate)
        {
            item.onRotate(detents);
            repaintFocusArea();
            return;
        }

        moveFocusBySteps(detents > 0 ? 1 : -1);
        return;
    }

    if (event.type != HardwareControlType::ButtonPressed)
        return;

    if (event.id == HardwareControlId::EncoderPress)
    {
        activateFocused();
        return;
    }

    if (event.id == HardwareControlId::EncoderLongPress)
    {
        Logger::info("FORGE7 EncoderNavigator: EncoderLongPress received");

        if (appContext != nullptr && appContext->tryConsumeEncoderLongPress != nullptr
            && appContext->tryConsumeEncoderLongPress())
            return;

        if (escapeHandler != nullptr)
            escapeHandler();
    }
}

void EncoderNavigator::moveFocusBySteps(const int deltaSteps)
{
    auto& chain = activeChain();
    const int n = static_cast<int>(chain.size());

    if (n <= 0 || deltaSteps == 0)
        return;

    focusIndex = (focusIndex + deltaSteps) % n;

    if (focusIndex < 0)
        focusIndex += n;

    triggerOnFocusForCurrentItem();
    repaint();
}

void EncoderNavigator::activateFocused()
{
    auto& chain = activeChain();

    if (focusIndex < 0 || focusIndex >= static_cast<int>(chain.size()))
        return;

    auto& item = chain[static_cast<size_t>(focusIndex)];

    if (item.onActivate != nullptr)
        item.onActivate();
}

void EncoderNavigator::triggerOnFocusForCurrentItem()
{
    auto& chain = activeChain();

    if (focusIndex < 0 || focusIndex >= static_cast<int>(chain.size()))
        return;

    auto& item = chain[static_cast<size_t>(focusIndex)];

    if (item.onFocus != nullptr)
        item.onFocus();
}

void EncoderNavigator::repaintFocusArea()
{
    repaint();
}

void EncoderNavigator::paint(juce::Graphics& g)
{
    if (!focusOverlayEnabled)
        return;

    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;

    const auto& chain = activeChain();

    if (chain.empty() || focusIndex < 0 || focusIndex >= static_cast<int>(chain.size()))
        return;

    const auto& item = chain[static_cast<size_t>(focusIndex)];

    if (item.hideNavigatorFocusRing)
        return;

    auto* target = item.target.getComponent();

    if (target == nullptr || !target->isVisible())
        return;

    const auto screen = target->getScreenBounds();
    const auto tl = parent->getLocalPoint(nullptr, screen.getTopLeft());
    const auto br = parent->getLocalPoint(nullptr, screen.getBottomRight());
    const auto rMain = juce::Rectangle<int>(tl, br);
    auto r = getLocalArea(parent, rMain).toFloat().expanded(kFocusRingExpand, kFocusRingExpand);

    g.setColour(focusRingColour().withAlpha(0.95f));
    g.drawRoundedRectangle(r, kFocusCornerRadius, kFocusStroke);

    g.setColour(focusRingColour().withAlpha(0.18f));
    g.fillRoundedRectangle(r, kFocusCornerRadius);
}

void EncoderNavigator::resized()
{
    repaint();
}

juce::String EncoderNavigator::getFocusDebugSummary() const
{
    const auto& chain = activeChain();

    if (chain.empty())
        return modalActive ? "Modal focus: (empty)" : "Focus: (empty)";

    const int n = static_cast<int>(chain.size());
    const int idx = juce::jlimit(0, n - 1, focusIndex);
    auto* t = chain[static_cast<size_t>(idx)].target.getComponent();
    const juce::String name = t != nullptr ? t->getName() : juce::String("(null)");

    return (modalActive ? juce::String("Modal ") : juce::String())
           + juce::String(idx + 1) + "/" + juce::String(n) + "  " + name;
}

} // namespace forge7
