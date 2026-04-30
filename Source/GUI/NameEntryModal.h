#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

struct AppContext;

/** Opaque card panel drawn behind title, editor, and keyboard. */
class NameEntryCard final : public juce::Component
{
public:
    void paint(juce::Graphics& g) override;
};

/** Outcome when attempting to commit a name (library save, future rename flows). */
enum class NameEntrySaveOutcome
{
    Success,
    NeedReplace,
    Failed
};

/** Centered in-app modal: title, text field, touch QWERTY, Cancel / Save.
    Supports EncoderNavigator modal focus chain and encoder long-press back.

    Normal operation must not use native AlertWindow or file dialogs.

    Use showSaveDialog() for flows that may require replace confirmation; use
    showPlainDialog() for simple confirm/cancel naming (no replace step). */
class NameEntryModal final : public juce::Component
{
public:
    using SaveAttemptHandler = std::function<NameEntrySaveOutcome(
        const juce::String& rawText, bool replaceIfExisting, juce::String& errorOut)>;

    explicit NameEntryModal(AppContext& context);
    ~NameEntryModal() override;

    /** Save flow: trySave may return NeedReplace to show in-modal Replace/Cancel. */
    static void showSaveDialog(AppContext& appContext,
                               const juce::String& title,
                               const juce::String& initialText,
                               SaveAttemptHandler trySave,
                               std::function<void()> onSaveSuccess = {});

    /** Simple name entry: Save calls onConfirm(trimmed text); no replace UI. */
    static void showPlainDialog(AppContext& appContext,
                                const juce::String& title,
                                const juce::String& initialText,
                                std::function<void(const juce::String& name)> onConfirm,
                                std::function<void()> onCancel = {});

    /** True if any in-app name dialog is visible (message thread). */
    static bool isAnyActiveInstanceVisible() noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentSizeChanged() override;

private:
    enum class Phase
    {
        Entry,
        ReplaceConfirm
    };

    AppContext& appContext;

    Phase phase = Phase::Entry;

    bool plainMode = false;
    SaveAttemptHandler saveAttempt;
    std::function<void()> onSaveSuccess;
    std::function<void(const juce::String&)> onPlainConfirm;
    std::function<void()> onPlainCancel;

    NameEntryCard card;

    juce::Label titleLabel;
    juce::TextEditor nameEditor;
    juce::Label statusLabel;
    juce::Label replaceMessageLabel;

    juce::TextButton cancelButton { "Cancel" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton replaceButton { "Replace" };
    juce::TextButton replaceCancelButton { "Cancel" };

    juce::OwnedArray<juce::TextButton> keyButtons;
    juce::TextButton spaceButton { "Space" };
    juce::TextButton backspaceButton { "Backspace" };
    juce::TextButton clearButton { "Clear" };

    void buildKeyboard();
    void layoutCard(juce::Rectangle<int> cardArea);
    void applyPhaseVisibility();
    void syncEncoderFocus();
    void attachEncoderLongPressHandler();
    void detachEncoderLongPressHandler();

    std::function<bool()> previousEncoderLongPressHandler;

    void dismissAsync();
    void handleCancelPressed();
    void handleSavePressed();
    void handleReplacePressed();
    void handleReplaceCancelPressed();

    void appendChar(juce::juce_wchar ch);
    void backspaceChar();
    void clearField();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NameEntryModal)
};

} // namespace forge7
