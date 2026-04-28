#include "MainComponent.h"

#include "AppContext.h"

#include "../Controls/KeyboardHardwareSimulator.h"
#include "../GUI/FullscreenPluginEditorComponent.h"
#include "../GUI/PerformanceViewComponent.h"
#include "../GUI/RackViewComponent.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"

namespace forge7
{

MainComponent::MainComponent(AppContext& context)
    : appContext(context)
{
    appContext.mainComponent = this;
    appContext.encoderNavigator = &encoderNavigator;
    encoderNavigator.attachContext(&appContext);

    encoderNavigator.setEscapeHandler([this]()
                                      {
                                          if (fullscreenPluginEditor != nullptr)
                                          {
                                              closeFullscreenPluginEditor();
                                              return;
                                          }

                                          if (rackView != nullptr && rackView->isPluginBrowserVisible())
                                          {
                                              rackView->closePluginBrowser();
                                              return;
                                          }

                                          if (editMode)
                                              setEditMode(false);
                                      });

    performanceView = std::make_unique<PerformanceViewComponent>(appContext);
    rackView = std::make_unique<RackViewComponent>(appContext);

    if (appContext.controlManager != nullptr)
    {
        keyboardHardwareSimulator = std::make_unique<KeyboardHardwareSimulator>(*appContext.controlManager);
        keyboardHardwareSimulator->attachTo(*this);
    }

    setWantsKeyboardFocus(true);

    grabKeyboardFocus();

    addAndMakeVisible(*performanceView);
    rackView->setVisible(false);
    addChildComponent(*rackView);
    addAndMakeVisible(encoderNavigator);

    setEditMode(false);
}

MainComponent::~MainComponent()
{
    closeFullscreenPluginEditor();
    appContext.encoderNavigator = nullptr;
    appContext.mainComponent = nullptr;
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.88f));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    if (fullscreenPluginEditor != nullptr && fullscreenPluginEditor->isShowing())
    {
        fullscreenPluginEditor->setBounds(bounds);
        encoderNavigator.setBounds(bounds);
        encoderNavigator.toFront(false);
        fullscreenPluginEditor->syncEncoderFocus();
        return;
    }

    if (performanceView != nullptr)
        performanceView->setBounds(bounds);
    if (rackView != nullptr)
        rackView->setBounds(bounds);

    encoderNavigator.setBounds(bounds);

    if (performanceView != nullptr)
        performanceView->syncEncoderFocus();
    if (rackView != nullptr)
        rackView->syncEncoderFocus();
}

void MainComponent::setEditMode(const bool shouldShowRackEditor)
{
    editMode = shouldShowRackEditor;

    if (!editMode && rackView != nullptr)
        rackView->closePluginBrowser();

    if (performanceView != nullptr)
        performanceView->setVisible(!editMode);
    if (rackView != nullptr)
    {
        rackView->setVisible(editMode);
        if (editMode)
            rackView->toFront(false);
        else
            performanceView->toFront(false);
    }

    encoderNavigator.toFront(false);

    if (fullscreenPluginEditor != nullptr)
        encoderNavigator.toFront(false);

    if (performanceView != nullptr)
        performanceView->syncEncoderFocus();
    if (rackView != nullptr)
        rackView->syncEncoderFocus();
}

void MainComponent::openFullscreenPluginEditor(const int slotIndex)
{
    if (!juce::isPositiveAndBelow(slotIndex, kPluginChainMaxSlots))
        return;

    closeFullscreenPluginEditor();

    fullscreenPluginEditor = std::make_unique<FullscreenPluginEditorComponent>(
        appContext,
        slotIndex,
        [this]()
        {
            closeFullscreenPluginEditor();
        });

    addAndMakeVisible(*fullscreenPluginEditor);
    fullscreenPluginEditor->toFront(false);
    encoderNavigator.toFront(false);
    resized();
}

void MainComponent::closeFullscreenPluginEditor()
{
    if (fullscreenPluginEditor == nullptr)
        return;

    removeChildComponent(fullscreenPluginEditor.get());
    fullscreenPluginEditor.reset();

    resized();

    if (rackView != nullptr)
        rackView->syncEncoderFocus();
    else if (performanceView != nullptr)
        performanceView->syncEncoderFocus();
}

void MainComponent::closeFullscreenPluginEditorIfShowingSlot(const int slotIndex)
{
    if (fullscreenPluginEditor != nullptr && fullscreenPluginEditor->getPluginSlotIndex() == slotIndex)
        closeFullscreenPluginEditor();
}

bool MainComponent::isFullscreenPluginEditorActive() const noexcept
{
    return fullscreenPluginEditor != nullptr;
}

void MainComponent::openPluginBrowserFromDevTools()
{
    setEditMode(true);

    if (rackView != nullptr)
        rackView->showPluginBrowser();
}

void MainComponent::focusPluginInspectorFromDevTools()
{
    setEditMode(true);

    if (rackView == nullptr || appContext.pluginHostManager == nullptr)
        return;

    auto* chain = appContext.pluginHostManager->getPluginChain();

    if (chain == nullptr)
        return;

    for (int i = 0; i < kPluginChainMaxSlots; ++i)
    {
        const auto info = chain->getSlotInfo(i);

        if (!info.isEmpty && !info.missingPlugin)
        {
            rackView->selectRackSlot(i);
            return;
        }
    }

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                             "Plugin inspector",
                                             "Load a plugin in the rack first - no non-empty slots found.",
                                             "OK");
}

juce::String MainComponent::describeUiSurfaceForDevTools() const
{
    if (fullscreenPluginEditor != nullptr)
        return "Fullscreen Plugin Editor";

    if (rackView != nullptr && rackView->isPluginBrowserVisible())
        return "Plugin Browser (fullscreen)";

    if (editMode)
        return "Edit Mode / Rack View";

    return "Performance Mode";
}

} // namespace forge7
