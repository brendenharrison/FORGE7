#include "UnsavedChangesModal.h"

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

std::vector<std::shared_ptr<UnsavedChangesModal>> gActiveUnsavedModals;

void registerActive(const std::shared_ptr<UnsavedChangesModal>& m)
{
    gActiveUnsavedModals.push_back(m);
}

void unregisterActive(UnsavedChangesModal* p)
{
    const auto it = std::remove_if(gActiveUnsavedModals.begin(),
                                   gActiveUnsavedModals.end(),
                                   [p](const std::shared_ptr<UnsavedChangesModal>& s)
                                   { return s.get() == p; });
    gActiveUnsavedModals.erase(it, gActiveUnsavedModals.end());
}

} // namespace

void UnsavedChangesModal::Card::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1b2028));
    g.setColour(uiAccent().withAlpha(0.55f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 2.0f);
}

UnsavedChangesModal::UnsavedChangesModal(AppContext& context)
    : appContext(context)
{
    setInterceptsMouseClicks(true, true);
    addAndMakeVisible(card);
    card.setInterceptsMouseClicks(true, true);

    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(19.0f));
    titleLabel.setColour(juce::Label::textColourId, uiText());
    card.addAndMakeVisible(titleLabel);

    messageLabel.setJustificationType(juce::Justification::centred);
    messageLabel.setFont(juce::Font(15.0f));
    messageLabel.setColour(juce::Label::textColourId, uiText());
    messageLabel.setMinimumHorizontalScale(0.75f);
    card.addAndMakeVisible(messageLabel);

    stylePrimaryButton(saveButton);
    saveButton.onClick = [this]()
    {
        auto cb = std::move(onChosen);
        dismissAsync();

        if (cb != nullptr)
            cb(UnsavedProjectChoice::Save);
    };
    card.addAndMakeVisible(saveButton);

    styleSecondaryButton(discardButton);
    discardButton.onClick = [this]()
    {
        auto cb = std::move(onChosen);
        dismissAsync();

        if (cb != nullptr)
            cb(UnsavedProjectChoice::Discard);
    };
    card.addAndMakeVisible(discardButton);

    styleSecondaryButton(cancelButton);
    cancelButton.onClick = [this]()
    {
        auto cb = std::move(onChosen);
        dismissAsync();

        if (cb != nullptr)
            cb(UnsavedProjectChoice::Cancel);
    };
    card.addAndMakeVisible(cancelButton);

    titleLabel.setText("Unsaved changes", juce::dontSendNotification);
    messageLabel.setText("Current project has unsaved changes.", juce::dontSendNotification);
}

void UnsavedChangesModal::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.62f));
}

void UnsavedChangesModal::resized()
{
    auto r = getLocalBounds();
    const int cw = juce::jmin(r.getWidth() - 80, 520);
    const int ch = juce::jmin(r.getHeight() - 60, 240);
    card.setBounds(r.withSizeKeepingCentre(cw, ch));

    auto area = card.getLocalBounds().reduced(20, 18);
    titleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(10);
    messageLabel.setBounds(area.removeFromTop(52));
    area.removeFromTop(16);

    auto row = area.removeFromTop(46);
    const int gap = 10;
    const int w3 = (row.getWidth() - gap * 2) / 3;
    saveButton.setBounds(row.removeFromLeft(w3).reduced(0, 2));
    row.removeFromLeft(gap);
    discardButton.setBounds(row.removeFromLeft(w3).reduced(0, 2));
    row.removeFromLeft(gap);
    cancelButton.setBounds(row.removeFromLeft(w3).reduced(0, 2));

    syncEncoderFocus();
}

void UnsavedChangesModal::parentSizeChanged()
{
    if (auto* p = getParentComponent())
        setBounds(p->getLocalBounds());
}

void UnsavedChangesModal::syncEncoderFocus()
{
    if (appContext.encoderNavigator == nullptr)
        return;

    std::vector<EncoderFocusItem> items;

    items.push_back({&saveButton,
                     [this]()
                     {
                         if (saveButton.isEnabled())
                             saveButton.triggerClick();
                     },
                     {}});

    items.push_back({&discardButton,
                     [this]()
                     {
                         if (discardButton.isEnabled())
                             discardButton.triggerClick();
                     },
                     {}});

    items.push_back({&cancelButton,
                     [this]()
                     {
                         if (cancelButton.isEnabled())
                             cancelButton.triggerClick();
                     },
                     {}});

    appContext.encoderNavigator->setModalFocusChain(std::move(items));
}

void UnsavedChangesModal::attachEncoderLongPressHandler()
{
    appContext.tryConsumeEncoderLongPress = [this]()
    {
        cancelButton.triggerClick();
        return true;
    };
}

void UnsavedChangesModal::detachEncoderLongPressHandler()
{
    appContext.tryConsumeEncoderLongPress = {};
}

void UnsavedChangesModal::dismissAsync()
{
    detachEncoderLongPressHandler();

    if (appContext.encoderNavigator != nullptr)
        appContext.encoderNavigator->clearModalFocusChain();

    juce::Component::SafePointer<UnsavedChangesModal> safe(this);
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

void UnsavedChangesModal::show(AppContext& appContextIn, std::function<void(UnsavedProjectChoice)> onChosenIn)
{
    if (appContextIn.mainComponent == nullptr)
        return;

    auto modal = std::make_shared<UnsavedChangesModal>(appContextIn);
    modal->onChosen = std::move(onChosenIn);

    registerActive(modal);

    auto& main = *appContextIn.mainComponent;
    main.addAndMakeVisible(modal.get());
    modal->setBounds(main.getLocalBounds());
    modal->toFront(true);

    if (appContextIn.encoderNavigator != nullptr)
        appContextIn.encoderNavigator->toFront(false);

    modal->attachEncoderLongPressHandler();
    modal->syncEncoderFocus();
}

} // namespace forge7
