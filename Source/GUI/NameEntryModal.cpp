#include "NameEntryModal.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "../App/AppContext.h"
#include "../App/MainComponent.h"
#include "../Controls/EncoderFocusTypes.h"
#include "../Controls/EncoderNavigator.h"

namespace forge7
{
namespace
{

juce::Colour uiText() noexcept
{
    return juce::Colour(0xffe8eaed);
}

juce::Colour uiMuted() noexcept
{
    return juce::Colour(0xff8a9099);
}

juce::Colour uiAccent() noexcept
{
    return juce::Colour(0xff6bc4ff);
}

void stylePrimaryButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, uiAccent().withAlpha(0.42f));
    b.setColour(juce::TextButton::textColourOffId, uiText());
    b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleSecondaryButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3140));
    b.setColour(juce::TextButton::textColourOffId, uiText());
    b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleKeyButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252b36));
    b.setColour(juce::TextButton::textColourOffId, uiText());
    b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

std::vector<std::shared_ptr<NameEntryModal>> gActiveNameEntryModals;

void registerActive(const std::shared_ptr<NameEntryModal>& m)
{
    gActiveNameEntryModals.push_back(m);
}

void unregisterActive(NameEntryModal* p)
{
    const auto it = std::remove_if(gActiveNameEntryModals.begin(),
                                   gActiveNameEntryModals.end(),
                                   [p](const std::shared_ptr<NameEntryModal>& s)
                                   { return s.get() == p; });
    gActiveNameEntryModals.erase(it, gActiveNameEntryModals.end());
}

} // namespace

void NameEntryCard::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1b2028));
    g.setColour(juce::Colour(0xff6bc4ff).withAlpha(0.55f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 2.0f);
}

NameEntryModal::NameEntryModal(AppContext& context)
    : appContext(context)
{
    setAlwaysOnTop(false);
    setInterceptsMouseClicks(true, true);

    addAndMakeVisible(card);
    card.setInterceptsMouseClicks(true, true);

    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(20.0f));
    titleLabel.setColour(juce::Label::textColourId, uiText());
    card.addAndMakeVisible(titleLabel);

    nameEditor.setMultiLine(false);
    nameEditor.setReturnKeyStartsNewLine(false);
    nameEditor.setScrollbarsShown(false);
    nameEditor.setFont(juce::Font(17.0f));
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff12161c));
    nameEditor.setColour(juce::TextEditor::textColourId, uiText());
    nameEditor.setColour(juce::TextEditor::outlineColourId, uiMuted());
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, uiAccent());
    nameEditor.setIndents(10, 6);
    nameEditor.setInputRestrictions(240, {});
    card.addAndMakeVisible(nameEditor);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::Font(13.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff8a80));
    statusLabel.setMinimumHorizontalScale(0.75f);
    card.addAndMakeVisible(statusLabel);

    replaceMessageLabel.setJustificationType(juce::Justification::centred);
    replaceMessageLabel.setFont(juce::Font(15.0f));
    replaceMessageLabel.setColour(juce::Label::textColourId, uiText());
    replaceMessageLabel.setText("Replace existing project?", juce::dontSendNotification);
    card.addAndMakeVisible(replaceMessageLabel);

    styleSecondaryButton(cancelButton);
    cancelButton.onClick = [this]() { handleCancelPressed(); };
    card.addAndMakeVisible(cancelButton);

    stylePrimaryButton(saveButton);
    saveButton.onClick = [this]() { handleSavePressed(); };
    card.addAndMakeVisible(saveButton);

    stylePrimaryButton(replaceButton);
    replaceButton.onClick = [this]() { handleReplacePressed(); };
    card.addAndMakeVisible(replaceButton);

    styleSecondaryButton(replaceCancelButton);
    replaceCancelButton.onClick = [this]() { handleReplaceCancelPressed(); };
    card.addAndMakeVisible(replaceCancelButton);

    styleKeyButton(spaceButton);
    spaceButton.onClick = [this]() { appendChar(' '); };
    card.addAndMakeVisible(spaceButton);

    styleKeyButton(backspaceButton);
    backspaceButton.onClick = [this]() { backspaceChar(); };
    card.addAndMakeVisible(backspaceButton);

    styleKeyButton(clearButton);
    clearButton.onClick = [this]() { clearField(); };
    card.addAndMakeVisible(clearButton);

    buildKeyboard();
}

NameEntryModal::~NameEntryModal()
{
    detachEncoderLongPressHandler();
}

void NameEntryModal::buildKeyboard()
{
    const char* rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};

    for (auto* row : rows)
    {
        for (const char* p = row; *p != '\0'; ++p)
        {
            char label[2] = {*p, '\0'};
            auto* b = keyButtons.add(new juce::TextButton(juce::String(label)));
            styleKeyButton(*b);
            const char c = *p;
            b->onClick = [this, c]()
            { appendChar(static_cast<juce::juce_wchar>(static_cast<unsigned char>(c))); };
            card.addAndMakeVisible(b);
        }
    }
}

void NameEntryModal::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.62f));
}

void NameEntryModal::resized()
{
    auto r = getLocalBounds();
    const int cw = juce::jmin(r.getWidth() - 80, 720);
    const int ch = juce::jmin(r.getHeight() - 60, 460);
    card.setBounds(r.withSizeKeepingCentre(cw, ch));
    layoutCard(card.getLocalBounds());
    syncEncoderFocus();
}

void NameEntryModal::parentSizeChanged()
{
    if (auto* p = getParentComponent())
        setBounds(p->getLocalBounds());
}

void NameEntryModal::layoutCard(juce::Rectangle<int> area)
{
    area.reduce(18, 16);

    auto titleR = area.removeFromTop(32);
    titleLabel.setBounds(titleR);
    area.removeFromTop(8);

    if (phase == Phase::ReplaceConfirm)
    {
        auto msgR = area.removeFromTop(juce::jmin(96, juce::jmax(44, area.getHeight() - 72)));
        replaceMessageLabel.setBounds(msgR);
        area.removeFromTop(16);
        auto btnRow = area.removeFromTop(48);
        const int rg = 12;
        auto leftR = btnRow.removeFromLeft((btnRow.getWidth() - rg) / 2);
        btnRow.removeFromLeft(rg);
        replaceButton.setBounds(leftR);
        replaceCancelButton.setBounds(btnRow);
        return;
    }

    auto editorR = area.removeFromTop(40);
    nameEditor.setBounds(editorR);
    area.removeFromTop(4);

    auto statusR = area.removeFromTop(22);
    statusLabel.setBounds(statusR);
    area.removeFromTop(8);

    const int bottomRowH = 44;
    const int keyGap = 6;
    const int row4H = juce::jlimit(38, 52, juce::roundToInt((float)area.getHeight() * 0.16f));

    auto bottomButtons = area.removeFromBottom(bottomRowH);
    const int gap = 10;
    auto cancelR = bottomButtons.removeFromLeft((bottomButtons.getWidth() - gap) / 2);
    bottomButtons.removeFromLeft(gap);
    cancelButton.setBounds(cancelR);
    saveButton.setBounds(bottomButtons);

    area.removeFromBottom(8);

    auto row4 = area.removeFromBottom(row4H);
    const int keyStep4 = (row4.getWidth() - keyGap * 2) / 3;
    spaceButton.setBounds(row4.removeFromLeft(keyStep4));
    row4.removeFromLeft(keyGap);
    backspaceButton.setBounds(row4.removeFromLeft(keyStep4));
    row4.removeFromLeft(keyGap);
    clearButton.setBounds(row4);

    area.removeFromBottom(keyGap);

    const int rowH = juce::jmax(34, (area.getHeight() - keyGap * 2) / 3);

    auto layoutRow = [&](juce::Rectangle<int> row, int startKeyIndex, int count)
    {
        if (count <= 0)
            return;

        const int slot = (row.getWidth() - keyGap * (count - 1)) / count;

        for (int i = 0; i < count; ++i)
        {
            if (auto* kb = keyButtons[static_cast<size_t>(startKeyIndex + i)])
                kb->setBounds(row.removeFromLeft(slot));

            if (i < count - 1)
                row.removeFromLeft(keyGap);
        }
    };

    auto r0 = area.removeFromTop(rowH);
    layoutRow(r0, 0, 10);
    area.removeFromTop(keyGap);

    auto r1 = area.removeFromTop(rowH);
    layoutRow(r1, 10, 9);
    area.removeFromTop(keyGap);

    auto r2 = area.removeFromTop(rowH);
    layoutRow(r2, 19, 7);
}

void NameEntryModal::applyPhaseVisibility()
{
    const bool entry = (phase == Phase::Entry);

    nameEditor.setVisible(entry);
    spaceButton.setVisible(entry);
    backspaceButton.setVisible(entry);
    clearButton.setVisible(entry);

    for (auto* b : keyButtons)
        if (b != nullptr)
            b->setVisible(entry);

    cancelButton.setVisible(entry);
    saveButton.setVisible(entry);

    replaceMessageLabel.setVisible(! entry);
    replaceButton.setVisible(! entry);
    replaceCancelButton.setVisible(! entry);

    if (entry)
        statusLabel.setVisible(statusLabel.getText().isNotEmpty());
    else
        statusLabel.setVisible(false);
}

void NameEntryModal::syncEncoderFocus()
{
    if (appContext.encoderNavigator == nullptr)
        return;

    std::vector<EncoderFocusItem> items;

    if (phase == Phase::ReplaceConfirm)
    {
        items.push_back({&replaceButton,
                         [this]()
                         {
                             if (replaceButton.isEnabled())
                                 handleReplacePressed();
                         },
                         {}});

        items.push_back({&replaceCancelButton,
                         [this]()
                         {
                             if (replaceCancelButton.isEnabled())
                                 handleReplaceCancelPressed();
                         },
                         {}});
    }
    else
    {
        items.push_back({&nameEditor,
                         [this]()
                         { nameEditor.grabKeyboardFocus(); },
                         {}});

        for (auto* b : keyButtons)
            if (b != nullptr)
                items.push_back(
                    {b,
                     [sb = juce::Component::SafePointer<juce::TextButton>(b)]()
                     {
                         if (sb != nullptr)
                             sb->triggerClick();
                     },
                     {}});

        items.push_back({&spaceButton,
                         [this]()
                         {
                             if (spaceButton.isEnabled())
                                 spaceButton.triggerClick();
                         },
                         {}});

        items.push_back({&backspaceButton,
                         [this]()
                         {
                             if (backspaceButton.isEnabled())
                                 backspaceButton.triggerClick();
                         },
                         {}});

        items.push_back({&clearButton,
                         [this]()
                         {
                             if (clearButton.isEnabled())
                                 clearButton.triggerClick();
                         },
                         {}});

        items.push_back({&cancelButton,
                         [this]()
                         {
                             if (cancelButton.isEnabled())
                                 cancelButton.triggerClick();
                         },
                         {}});

        items.push_back({&saveButton,
                         [this]()
                         {
                             if (saveButton.isEnabled())
                                 saveButton.triggerClick();
                         },
                         {}});
    }

    appContext.encoderNavigator->setModalFocusChain(std::move(items));
}

void NameEntryModal::attachEncoderLongPressHandler()
{
    appContext.tryConsumeEncoderLongPress = [this]()
    {
        if (phase == Phase::ReplaceConfirm)
        {
            phase = Phase::Entry;
            statusLabel.setText({}, juce::dontSendNotification);
            applyPhaseVisibility();
            resized();
            syncEncoderFocus();
            return true;
        }

        handleCancelPressed();
        return true;
    };
}

void NameEntryModal::detachEncoderLongPressHandler()
{
    appContext.tryConsumeEncoderLongPress = {};
}

void NameEntryModal::appendChar(const juce::juce_wchar ch)
{
    nameEditor.moveCaretToEnd();
    nameEditor.insertTextAtCaret(juce::String() + ch);
}

void NameEntryModal::backspaceChar()
{
    auto t = nameEditor.getText();

    if (t.isEmpty())
        return;

    t = t.substring(0, t.length() - 1);
    nameEditor.setText(t);
    nameEditor.moveCaretToEnd();
}

void NameEntryModal::clearField()
{
    nameEditor.clear();
    nameEditor.grabKeyboardFocus();
}

void NameEntryModal::dismissAsync()
{
    detachEncoderLongPressHandler();

    if (appContext.encoderNavigator != nullptr)
        appContext.encoderNavigator->clearModalFocusChain();

    juce::Component::SafePointer<NameEntryModal> safe(this);
    juce::MessageManager::callAsync(
        [safe]()
        {
            auto* raw = safe.getComponent();

            if (raw == nullptr)
                return;

            if (raw->getParentComponent() != nullptr)
                raw->getParentComponent()->removeChildComponent(raw);

            unregisterActive(raw);
        });
}

void NameEntryModal::handleCancelPressed()
{
    if (phase == Phase::ReplaceConfirm)
    {
        phase = Phase::Entry;
        statusLabel.setText({}, juce::dontSendNotification);
        applyPhaseVisibility();
        resized();
        syncEncoderFocus();
        return;
    }

    if (plainMode && onPlainCancel)
        onPlainCancel();

    dismissAsync();
}

void NameEntryModal::handleSavePressed()
{
    if (plainMode)
    {
        const auto t = nameEditor.getText().trim();

        if (t.isEmpty())
        {
            statusLabel.setText("Enter a name.", juce::dontSendNotification);
            applyPhaseVisibility();
            resized();
            return;
        }

        if (onPlainConfirm)
            onPlainConfirm(t);

        dismissAsync();
        return;
    }

    juce::String err;

    if (! saveAttempt)
    {
        dismissAsync();
        return;
    }

    const auto r = saveAttempt(nameEditor.getText(), false, err);

    if (r == NameEntrySaveOutcome::Success)
    {
        if (onSaveSuccess != nullptr)
            onSaveSuccess();

        dismissAsync();
        return;
    }

    if (r == NameEntrySaveOutcome::NeedReplace)
    {
        phase = Phase::ReplaceConfirm;
        statusLabel.setText({}, juce::dontSendNotification);
        applyPhaseVisibility();
        resized();
        syncEncoderFocus();
        return;
    }

    statusLabel.setText(err.isNotEmpty() ? err : juce::String("Could not save."), juce::dontSendNotification);
    applyPhaseVisibility();
    resized();
    syncEncoderFocus();
}

void NameEntryModal::handleReplacePressed()
{
    juce::String err;

    if (! saveAttempt)
    {
        dismissAsync();
        return;
    }

    const auto r = saveAttempt(nameEditor.getText(), true, err);

    if (r == NameEntrySaveOutcome::Success)
    {
        if (onSaveSuccess != nullptr)
            onSaveSuccess();

        dismissAsync();
        return;
    }

    phase = Phase::Entry;
    statusLabel.setText(err.isNotEmpty() ? err : juce::String("Could not save."), juce::dontSendNotification);
    applyPhaseVisibility();
    resized();
    syncEncoderFocus();
}

void NameEntryModal::handleReplaceCancelPressed()
{
    phase = Phase::Entry;
    statusLabel.setText({}, juce::dontSendNotification);
    applyPhaseVisibility();
    resized();
    syncEncoderFocus();
}

void NameEntryModal::showSaveDialog(AppContext& appContextIn,
                                    const juce::String& title,
                                    const juce::String& initialText,
                                    SaveAttemptHandler trySave,
                                    std::function<void()> onSaveSuccessIn)
{
    if (appContextIn.mainComponent == nullptr)
        return;

    auto modal = std::make_shared<NameEntryModal>(appContextIn);
    modal->plainMode = false;
    modal->phase = Phase::Entry;
    modal->saveAttempt = std::move(trySave);
    modal->onSaveSuccess = std::move(onSaveSuccessIn);
    modal->titleLabel.setText(title, juce::dontSendNotification);
    modal->nameEditor.setText(initialText, juce::dontSendNotification);
    modal->statusLabel.setText({}, juce::dontSendNotification);
    modal->applyPhaseVisibility();

    registerActive(modal);

    auto& main = *appContextIn.mainComponent;
    main.addAndMakeVisible(modal.get());
    modal->setBounds(main.getLocalBounds());
    modal->toFront(true);

    if (appContextIn.encoderNavigator != nullptr)
        appContextIn.encoderNavigator->toFront(false);

    modal->attachEncoderLongPressHandler();
    modal->grabKeyboardFocus();
    modal->nameEditor.grabKeyboardFocus();
    modal->syncEncoderFocus();
}

void NameEntryModal::showPlainDialog(AppContext& appContextIn,
                                     const juce::String& title,
                                     const juce::String& initialText,
                                     std::function<void(const juce::String&)> onConfirm,
                                     std::function<void()> onCancel)
{
    if (appContextIn.mainComponent == nullptr)
        return;

    auto modal = std::make_shared<NameEntryModal>(appContextIn);
    modal->plainMode = true;
    modal->phase = Phase::Entry;
    modal->onPlainConfirm = std::move(onConfirm);
    modal->onPlainCancel = std::move(onCancel);
    modal->titleLabel.setText(title, juce::dontSendNotification);
    modal->nameEditor.setText(initialText, juce::dontSendNotification);
    modal->statusLabel.setText({}, juce::dontSendNotification);
    modal->applyPhaseVisibility();

    registerActive(modal);

    auto& main = *appContextIn.mainComponent;
    main.addAndMakeVisible(modal.get());
    modal->setBounds(main.getLocalBounds());
    modal->toFront(true);

    if (appContextIn.encoderNavigator != nullptr)
        appContextIn.encoderNavigator->toFront(false);

    modal->attachEncoderLongPressHandler();
    modal->grabKeyboardFocus();
    modal->nameEditor.grabKeyboardFocus();
    modal->syncEncoderFocus();
}

} // namespace forge7
