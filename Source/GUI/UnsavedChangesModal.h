#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

struct AppContext;

enum class UnsavedProjectChoice
{
    Save,
    Discard,
    Cancel
};

/** Centered in-app modal: unsaved project warning with Save / Discard / Cancel (touch + encoder). */
class UnsavedChangesModal final : public juce::Component
{
public:
    explicit UnsavedChangesModal(AppContext& context);

    static void show(AppContext& appContext, std::function<void(UnsavedProjectChoice)> onChosen);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentSizeChanged() override;

private:
    class Card final : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
    };

    void syncEncoderFocus();
    void attachEncoderLongPressHandler();
    void detachEncoderLongPressHandler();
    void dismissAsync();

    AppContext& appContext;

    Card card;
    juce::Label titleLabel;
    juce::Label messageLabel;

    juce::TextButton saveButton { "Save" };
    juce::TextButton discardButton { "Discard" };
    juce::TextButton cancelButton { "Cancel" };

    std::function<void(UnsavedProjectChoice)> onChosen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UnsavedChangesModal)
};

} // namespace forge7
