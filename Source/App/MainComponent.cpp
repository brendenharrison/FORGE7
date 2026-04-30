#include "MainComponent.h"

#include "AppContext.h"
#include "AppConfig.h"
#include "ProjectSession.h"

#include "../Audio/AudioEngine.h"
#include "../Controls/ChainChordDetector.h"
#include "../Controls/HardwareControlTypes.h"
#include "../Controls/KeyboardHardwareSimulator.h"
#include "../GUI/FullscreenPluginEditorComponent.h"
#include "../GUI/PerformanceViewComponent.h"
#include "../GUI/RackViewComponent.h"
#include "../GUI/ProjectSceneBrowserComponent.h"
#include "../GUI/SettingsComponent.h"
#include "../GUI/SimulatedControlsComponent.h"
#include "../GUI/TunerOverlayComponent.h"
#include "../GUI/NameEntryModal.h"
#include "../GUI/UnsavedChangesModal.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../Utilities/Logger.h"

namespace forge7
{

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
void MainComponent::SimHwDrawer::paint(juce::Graphics& g)
{
    // Dark drawer background so it reads as a panel.
    g.fillAll(juce::Colour(0xff0f1217));

    g.setColour(juce::Colour(0xff1b2028));
    g.drawRect(getLocalBounds(), 1);
}
#endif

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

                                          if (settingsComponent != nullptr)
                                          {
                                              closeSettings();
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

    chainChordDetector = std::make_unique<ChainChordDetector>(
        [this]() { toggleTunerOverlay(); },
        [this]()
        {
            if (appContext.projectSession != nullptr)
                appContext.projectSession->previousChain();

            refreshProjectDependentViews();
        },
        [this]()
        {
            if (appContext.projectSession != nullptr)
                appContext.projectSession->nextChain();

            refreshProjectDependentViews();
        });

    if (appContext.controlManager != nullptr)
    {
        appContext.controlManager->setChainChordConsumer(
            [this](const HardwareControlEvent& e) -> bool
            {
                if (e.type != HardwareControlType::ButtonPressed)
                    return false;

                if (e.id == HardwareControlId::ChainPreviousButton)
                {
                    if (chainChordDetector != nullptr)
                        chainChordDetector->chainPreviousClicked();

                    return true;
                }

                if (e.id == HardwareControlId::ChainNextButton)
                {
                    if (chainChordDetector != nullptr)
                        chainChordDetector->chainNextClicked();

                    return true;
                }

                return false;
            });

        keyboardHardwareSimulator = std::make_unique<KeyboardHardwareSimulator>(*appContext.controlManager);
        keyboardHardwareSimulator->attachTo(*this);
    }

    setWantsKeyboardFocus(true);

    grabKeyboardFocus();

    addAndMakeVisible(*performanceView);
    rackView->setVisible(false);
    addChildComponent(*rackView);
    addAndMakeVisible(encoderNavigator);

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    // In-app simulated controls drawer (fallback when DevToolsWindow is not visible).
    simulatedControlsDrawer.setAlwaysOnTop(true);
    simulatedControlsDrawer.setVisible(false);
    simulatedControlsDrawer.setInterceptsMouseClicks(true, true);
    addChildComponent(simulatedControlsDrawer);

    simulatedControlsTitleLabel.setText("Sim Hardware", juce::dontSendNotification);
    simulatedControlsTitleLabel.setJustificationType(juce::Justification::centredLeft);
    simulatedControlsTitleLabel.setFont(juce::Font(15.0f));
    simulatedControlsTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe8eaed));
    simulatedControlsDrawer.addAndMakeVisible(simulatedControlsTitleLabel);

    simulatedControlsHideButton.onClick = [this]() { hideSimulatedControlsPanel(); };
    simulatedControlsDrawer.addAndMakeVisible(simulatedControlsHideButton);

    simulatedControlsPanel = std::make_unique<SimulatedControlsComponent>(appContext);
    simulatedControlsViewport = std::make_unique<juce::Viewport>();
    simulatedControlsViewport->setScrollBarsShown(true, false);
    simulatedControlsViewport->setViewedComponent(simulatedControlsPanel.get(), false);
    simulatedControlsDrawer.addAndMakeVisible(*simulatedControlsViewport);

    Logger::info("FORGE7 SimHW: in-app drawer available (FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW=1).");
#endif

    setEditMode(false);

    appContext.tryConsumeEncoderLongPress = [this]()
    {
        return handleGlobalEncoderLongPress();
    };
    Logger::info("FORGE7 MainComponent: registered global encoder long press handler");
}

MainComponent::~MainComponent()
{
    hideTunerOverlay();

    if (appContext.controlManager != nullptr)
        appContext.controlManager->setChainChordConsumer({});

    closeFullscreenPluginEditor();
    closeProjectSceneJumpBrowser();
    appContext.tryConsumeEncoderLongPress = {};
    appContext.encoderNavigator = nullptr;
    appContext.mainComponent = nullptr;
}

bool MainComponent::isModalOverlayOpen() const noexcept
{
    if (settingsComponent != nullptr && settingsComponent->isShowing())
        return true;

    if (tunerOverlay != nullptr && tunerOverlay->isVisible())
        return true;

    if (projectSceneJumpBrowser != nullptr && projectSceneJumpBrowser->isVisible())
        return true;

    if (fullscreenPluginEditor != nullptr && fullscreenPluginEditor->isShowing())
        return true;

    if (rackView != nullptr && rackView->isPluginBrowserVisible())
        return true;

    if (NameEntryModal::isAnyActiveInstanceVisible())
        return true;

    if (UnsavedChangesModal::isAnyActiveInstanceVisible())
        return true;

    return false;
}

void MainComponent::refreshProjectDependentViews()
{
    if (performanceView != nullptr)
        performanceView->refreshHud();

    if (rackView != nullptr)
        rackView->refreshAfterProjectHydration();
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

    if (settingsComponent != nullptr && settingsComponent->isShowing())
    {
        settingsComponent->setBounds(bounds);
        settingsComponent->toFront(false);
        encoderNavigator.setBounds(bounds);
        encoderNavigator.toFront(false);
        settingsComponent->syncEncoderFocus();
        return;
    }

    if (projectSceneJumpBrowser != nullptr && projectSceneJumpBrowser->isVisible())
    {
        projectSceneJumpBrowser->setBounds(bounds);
        encoderNavigator.setBounds(bounds);
        projectSceneJumpBrowser->toFront(false);
        encoderNavigator.toFront(false);
        return;
    }

    if (tunerOverlay != nullptr && tunerOverlay->isVisible())
    {
        tunerOverlay->setBounds(bounds);
        tunerOverlay->toFront(false);
        encoderNavigator.setBounds(bounds);
        encoderNavigator.toFront(false);
        return;
    }

    if (performanceView != nullptr)
        performanceView->setBounds(bounds);
    if (rackView != nullptr)
        rackView->setBounds(bounds);

    encoderNavigator.setBounds(bounds);

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    if (simulatedControlsDrawer.isVisible())
    {
        const int w = juce::jlimit(320, 380, juce::roundToInt((float)getWidth() * 0.30f));
        auto drawerArea = bounds.removeFromRight(w);

        simulatedControlsDrawer.setBounds(drawerArea);
        simulatedControlsDrawer.toFront(false);

        auto inner = simulatedControlsDrawer.getLocalBounds().reduced(10);
        auto topRow = inner.removeFromTop(34);
        simulatedControlsTitleLabel.setBounds(topRow.removeFromLeft(juce::jmax(120, topRow.getWidth() - 100)));
        simulatedControlsHideButton.setBounds(topRow.removeFromRight(92).reduced(0, 4));
        inner.removeFromTop(6);

        if (simulatedControlsViewport != nullptr)
        {
            simulatedControlsViewport->setBounds(inner);
            simulatedControlsViewport->setScrollBarsShown(true, false);
        }

        // Viewport doesn't size its viewed component; we must set an explicit size so content is visible + scrollable.
        if (simulatedControlsPanel != nullptr && simulatedControlsViewport != nullptr)
        {
            const int contentW = juce::jmax(280, simulatedControlsViewport->getWidth() - 18);
            const int contentH = juce::jmax(920, simulatedControlsViewport->getHeight());
            simulatedControlsPanel->setSize(contentW, contentH);
        }
    }
    else
    {
        simulatedControlsDrawer.setBounds({});
    }
#endif

    if (NameEntryModal::isAnyActiveInstanceVisible() || UnsavedChangesModal::isAnyActiveInstanceVisible())
    {
        Logger::info("FORGE7 Focus: ignored background focus sync because modal overlay is open");
        encoderNavigator.toFront(false);
        return;
    }

    if (performanceView != nullptr)
        performanceView->syncEncoderFocus();
    if (rackView != nullptr)
        rackView->syncEncoderFocus();
}

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
void MainComponent::toggleSimulatedControlsPanel()
{
    if (simulatedControlsPanelVisible)
        hideSimulatedControlsPanel();
    else
        showSimulatedControlsPanel();
}

void MainComponent::showSimulatedControlsPanel()
{
    simulatedControlsPanelVisible = true;
    simulatedControlsDrawer.setVisible(true);
    simulatedControlsDrawer.toFront(true);
    resized();

    Logger::info("FORGE7 SimHW: drawer bounds=" + simulatedControlsDrawer.getBounds().toString());
    if (simulatedControlsViewport != nullptr)
        Logger::info("FORGE7 SimHW: viewport bounds=" + simulatedControlsViewport->getBounds().toString());
    if (simulatedControlsPanel != nullptr)
        Logger::info("FORGE7 SimHW: content size=" + juce::String(simulatedControlsPanel->getWidth())
                     + "x" + juce::String(simulatedControlsPanel->getHeight()));
}

void MainComponent::hideSimulatedControlsPanel()
{
    simulatedControlsPanelVisible = false;
    simulatedControlsDrawer.setVisible(false);
    resized();

    Logger::info("FORGE7 SimHW: hiding in-app drawer");
}
#endif

void MainComponent::openSettings()
{
    if (settingsComponent != nullptr)
        return;

    Logger::info("FORGE7 Focus: clearing background focus for Settings");
    encoderNavigator.clearAllFocus(true);

    settingsReturnToEditMode = editMode;

    settingsComponent = std::make_unique<SettingsComponent>(appContext, [this]() { closeSettings(); });
    addAndMakeVisible(*settingsComponent);
    settingsComponent->toFront(true);
    resized();
}

void MainComponent::closeSettings()
{
    if (settingsComponent == nullptr)
        return;

    removeChildComponent(settingsComponent.get());
    settingsComponent.reset();

    setEditMode(settingsReturnToEditMode);
    resized();
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

    if (!isModalOverlayOpen())
    {
        if (performanceView != nullptr)
            performanceView->syncEncoderFocus();
        if (rackView != nullptr)
            rackView->syncEncoderFocus();
    }
}

void MainComponent::openFullscreenPluginEditor(const int slotIndex)
{
    if (!juce::isPositiveAndBelow(slotIndex, kPluginChainMaxSlots))
        return;

    closeFullscreenPluginEditor();

    encoderNavigator.clearAllFocus(true);

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

bool MainComponent::isProjectSceneJumpBrowserOpen() const noexcept
{
    return projectSceneJumpBrowser != nullptr && projectSceneJumpBrowser->isVisible();
}

bool MainComponent::handleGlobalEncoderLongPress()
{
    Logger::info("FORGE7 MainComponent: tryConsumeEncoderLongPress called");

    if (isTunerOverlayVisible())
    {
        hideTunerOverlay();
        return true;
    }

    if (projectSceneJumpBrowser != nullptr && projectSceneJumpBrowser->isVisible())
    {
        Logger::info("FORGE7 JumpBrowser: close from encoder long press");
        closeProjectSceneJumpBrowser();
        return true;
    }

    if (fullscreenPluginEditor != nullptr && fullscreenPluginEditor->isShowing())
    {
        closeFullscreenPluginEditor();
        return true;
    }

    if (settingsComponent != nullptr && settingsComponent->isShowing())
    {
        closeSettings();
        return true;
    }

    if (rackView != nullptr && rackView->isPluginBrowserVisible())
    {
        rackView->closePluginBrowser();
        return true;
    }

    Logger::info("FORGE7 JumpBrowser: open from encoder long press");
    showProjectSceneJumpBrowser();
    return true;
}

void MainComponent::showProjectSceneJumpBrowser()
{
    appContext.projectSceneJumpBrowserOpen = true;

    encoderNavigator.clearAllFocus(true);

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    simulatedControlsDrawer.setAlwaysOnTop(false);
#endif

    if (projectSceneJumpBrowser == nullptr)
    {
        projectSceneJumpBrowser = std::make_unique<ProjectSceneBrowserComponent>(
            appContext,
            [this]()
            {
                closeProjectSceneJumpBrowser();
            });

        addChildComponent(*projectSceneJumpBrowser);
    }

    projectSceneJumpBrowser->setVisible(true);
    resized();

    projectSceneJumpBrowser->rescanAndRebuild();
    projectSceneJumpBrowser->onBrowserShown();

    projectSceneJumpBrowser->toFront(false);
    encoderNavigator.toFront(false);

    Logger::info("FORGE7 JumpBrowser: open bounds=" + getLocalBounds().toString());
}

void MainComponent::closeProjectSceneJumpBrowser()
{
    if (projectSceneJumpBrowser == nullptr || ! projectSceneJumpBrowser->isVisible())
        return;

    projectSceneJumpBrowser->setVisible(false);
    appContext.projectSceneJumpBrowserOpen = false;

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    if (simulatedControlsPanelVisible)
        simulatedControlsDrawer.setAlwaysOnTop(true);
#endif

    if (appContext.encoderNavigator != nullptr)
        appContext.encoderNavigator->clearModalFocusChain();

    if (performanceView != nullptr)
        performanceView->syncEncoderFocus();

    if (rackView != nullptr)
        rackView->syncEncoderFocus();

    resized();
}

void MainComponent::handleChainPreviousFromUi()
{
    if (chainChordDetector != nullptr)
        chainChordDetector->chainPreviousClicked();
}

void MainComponent::handleChainNextFromUi()
{
    if (chainChordDetector != nullptr)
        chainChordDetector->chainNextClicked();
}

bool MainComponent::isTunerOverlayVisible() const noexcept
{
    return tunerOverlay != nullptr && tunerOverlay->isVisible();
}

void MainComponent::toggleTunerOverlay()
{
    if (isTunerOverlayVisible())
        hideTunerOverlay();
    else
        showTunerOverlay();
}

void MainComponent::showTunerOverlay()
{
    Logger::info("FORGE7 Focus: clearing background focus for Tuner");
    encoderNavigator.clearAllFocus(true);

    if (tunerOverlay == nullptr)
        tunerOverlay = std::make_unique<TunerOverlayComponent>(
            appContext,
            [this]() { hideTunerOverlay(); });

    addAndMakeVisible(*tunerOverlay);

    if (appContext.audioEngine != nullptr)
    {
        appContext.audioEngine->setTunerCaptureActive(true);

        if (appContext.appConfig != nullptr)
            appContext.audioEngine->setTunerMutesOutput(appContext.appConfig->getTunerMutesOutput());
    }

    tunerOverlay->toFront(false);
    resized();
    encoderNavigator.toFront(false);
}

void MainComponent::hideTunerOverlay()
{
    if (tunerOverlay == nullptr)
        return;

    removeChildComponent(tunerOverlay.get());
    tunerOverlay.reset();

    if (appContext.audioEngine != nullptr)
        appContext.audioEngine->setTunerCaptureActive(false);

    if (appContext.encoderNavigator != nullptr)
        appContext.encoderNavigator->clearModalFocusChain();

    resized();

    if (performanceView != nullptr)
        performanceView->syncEncoderFocus();

    if (rackView != nullptr)
        rackView->syncEncoderFocus();
}

juce::String MainComponent::describeUiSurfaceForDevTools() const
{
    if (fullscreenPluginEditor != nullptr)
        return "Fullscreen Plugin Editor";

    if (isTunerOverlayVisible())
        return "Tuner";

    if (isProjectSceneJumpBrowserOpen())
        return "Jump Browser";

 #if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    if (simulatedControlsPanelVisible)
        return (editMode ? "Edit Mode / Rack View" : "Performance Mode") + juce::String(" (Sim HW drawer)");
 #endif

    if (rackView != nullptr && rackView->isPluginBrowserVisible())
        return "Plugin Browser (fullscreen)";

    if (editMode)
        return "Edit Mode / Rack View";

    return "Performance Mode";
}

} // namespace forge7
