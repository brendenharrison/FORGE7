#include "PluginEditorHostWindow.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace forge7
{
namespace
{
juce::Colour hostWindowBackground() noexcept
{
    return juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
}
} // namespace

std::unique_ptr<PluginEditorHostWindow> PluginEditorHostWindow::tryCreate(juce::AudioPluginInstance& plugin,
                                                                           const juce::String& windowTitle,
                                                                           std::function<void()> onWindowFullyClosed)
{
    if (!plugin.hasEditor())
        return nullptr;

    juce::AudioProcessorEditor* editor = plugin.createEditorIfNeeded();

    if (editor == nullptr)
        return nullptr;

    return std::unique_ptr<PluginEditorHostWindow>(
        new PluginEditorHostWindow(windowTitle, editor, std::move(onWindowFullyClosed)));
}

PluginEditorHostWindow::PluginEditorHostWindow(const juce::String& title,
                                                 juce::AudioProcessorEditor* editor,
                                                 std::function<void()> onWindowFullyClosed)
    : juce::DocumentWindow(title, hostWindowBackground(), DocumentWindow::closeButton, false)
    , onClosed(std::move(onWindowFullyClosed))
{
    jassert(editor != nullptr);
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    setUsingNativeTitleBar(true);
    setContentOwned(editor, true);

    const bool canResize = editor->isResizable();
    setResizable(canResize, canResize);

    if (canResize)
    {
        if (auto* c = editor->getConstrainer())
            setConstrainer(c);

        setResizeLimits(juce::jmax(80, editor->getWidth() / 4),
                        juce::jmax(60, editor->getHeight() / 4),
                        juce::jmax(editor->getWidth(), 8000),
                        juce::jmax(editor->getHeight(), 8000));
    }

    setVisible(true);
    centreWithSize(getWidth(), getHeight());
}

PluginEditorHostWindow::~PluginEditorHostWindow()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
}

void PluginEditorHostWindow::closeButtonPressed()
{
    /** Defer teardown so we never destroy this window synchronously from nested OS window callbacks. */
    auto cb = std::move(onClosed);
    onClosed = {};

    juce::MessageManager::callAsync([cb = std::move(cb)]() mutable
                                    {
                                        if (cb != nullptr)
                                            cb();
                                    });
}

} // namespace forge7
