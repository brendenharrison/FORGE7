#include "PluginInspectorComponent.h"

#include <algorithm>

#include "../App/AppContext.h"
#include "../App/MainComponent.h"
#include "../GUI/RackViewComponent.h"
#include "../Controls/HardwareControlTypes.h"
#include "../Controls/ParameterMappingDescriptor.h"
#include "../Controls/ParameterMappingManager.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"

namespace forge7
{
namespace
{
juce::Colour panelBg() noexcept { return juce::Colour(0xff161a20); }
juce::Colour panelStroke() noexcept { return juce::Colour(0xff4a9eff).withAlpha(0.22f); }
juce::Colour textColour() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour mutedColour() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour accentColour() noexcept { return juce::Colour(0xff4a9eff); }

void styleSecondaryButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, panelBg().brighter(0.12f));
    b.setColour(juce::TextButton::textColourOffId, textColour());
}

juce::String hardwareShortName(const HardwareControlId id)
{
    switch (id)
    {
        case HardwareControlId::Knob1:
            return "K1";
        case HardwareControlId::Knob2:
            return "K2";
        case HardwareControlId::Knob3:
            return "K3";
        case HardwareControlId::Knob4:
            return "K4";
        case HardwareControlId::AssignButton1:
            return "Assign 1";
        case HardwareControlId::AssignButton2:
            return "Assign 2";
        default:
            return {};
    }
}

const ParameterMappingDescriptor* findMappingForParameter(const juce::Array<ParameterMappingDescriptor>& rows,
                                                         const juce::String& sceneId,
                                                         const juce::String& variationId,
                                                         const int slotIndex,
                                                         const AutomatableParameterSummary& param)
{
    for (const auto& m : rows)
    {
        if (m.sceneId != sceneId || m.chainVariationId != variationId)
            continue;

        if (m.pluginSlotIndex != slotIndex)
            continue;

        if (param.parameterId.isNotEmpty() && m.pluginParameterId == param.parameterId)
            return &m;

        if (param.parameterIndex >= 0 && m.pluginParameterIndex == param.parameterIndex)
            return &m;
    }

    return nullptr;
}

void removeAllMappingsForParameter(ParameterMappingManager& mgr,
                                   const juce::String& sceneId,
                                   const juce::String& variationId,
                                   const int slotIndex,
                                   const AutomatableParameterSummary& param)
{
    const auto all = mgr.getAllMappings();

    for (const auto& m : all)
    {
        if (m.sceneId != sceneId || m.chainVariationId != variationId)
            continue;

        if (m.pluginSlotIndex != slotIndex)
            continue;

        bool match = false;

        if (param.parameterId.isNotEmpty() && m.pluginParameterId == param.parameterId)
            match = true;
        else if (param.parameterId.isEmpty() && m.pluginParameterIndex == param.parameterIndex)
            match = true;

        if (match)
            mgr.removeMapping(m);
    }
}

bool parameterLooksBoolean(const juce::AudioProcessorParameter* p)
{
    if (p == nullptr)
        return false;

    return p->getNumSteps() == 2;
}

} // namespace

//==============================================================================
class PluginInspectorComponent::ParameterRow final : public juce::Component
{
public:
    ParameterRow(PluginInspectorComponent& ownerIn,
                 AppContext& ctx,
                 const int slotIndexIn,
                 AutomatableParameterSummary summaryIn)
        : owner(ownerIn)
        , appContext(ctx)
        , slotIndex(slotIndexIn)
        , summary(std::move(summaryIn))
    {
        nameLabel.setJustificationType(juce::Justification::centredLeft);
        nameLabel.setFont(juce::Font(14.0f));
        nameLabel.setColour(juce::Label::textColourId, textColour());
        nameLabel.setText(summary.name.isNotEmpty() ? summary.name : juce::String("Parameter"), juce::dontSendNotification);
        addAndMakeVisible(nameLabel);

        valueLabel.setJustificationType(juce::Justification::centredRight);
        valueLabel.setFont(juce::Font(13.0f));
        valueLabel.setColour(juce::Label::textColourId, mutedColour());
        addAndMakeVisible(valueLabel);

        assignButton.setButtonText("Assign…");
        styleSecondaryButton(assignButton);
        assignButton.onClick = [this]() { showAssignMenu(); };
        addAndMakeVisible(assignButton);

        refreshAssignButtonLabel();
        refreshValueText();
    }

    void refreshValueText()
    {
        juce::String txt { "—" };

        if (appContext.pluginHostManager != nullptr)
        {
            if (auto* chain = appContext.pluginHostManager->getPluginChain())
            {
                if (auto* slot = chain->getSlot(static_cast<size_t>(slotIndex)))
                {
                    if (auto* instance = slot->getHostedInstance())
                    {
                        auto params = instance->getParameters();

                        if (summary.parameterIndex >= 0 && summary.parameterIndex < params.size())
                        {
                            if (auto* p = params[static_cast<size_t>(summary.parameterIndex)])
                                txt = p->getCurrentValueAsText();
                        }
                    }
                }
            }
        }

        valueLabel.setText(txt, juce::dontSendNotification);
    }

    void refreshAssignButtonLabel()
    {
        if (appContext.parameterMappingManager == nullptr || appContext.sceneManager == nullptr)
        {
            assignButton.setButtonText("Assign…");
            return;
        }

        auto* scene = appContext.sceneManager->getActiveScene();

        if (scene == nullptr)
        {
            assignButton.setButtonText("Assign…");
            return;
        }

        scene->clampActiveVariationIndex();

        const auto& vars = scene->getVariations();
        const int vi = scene->getActiveChainVariationIndex();

        if (!juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) || vars[static_cast<size_t>(vi)] == nullptr)
        {
            assignButton.setButtonText("Assign…");
            return;
        }

        const juce::String sceneId = scene->getSceneId();
        const juce::String varId = vars[static_cast<size_t>(vi)]->getVariationId();

        const auto rows = appContext.parameterMappingManager->getAllMappings();

        if (const auto* found = findMappingForParameter(rows, sceneId, varId, slotIndex, summary))
            assignButton.setButtonText(juce::String("Mapped: ") + hardwareShortName(found->hardwareControlId));
        else
            assignButton.setButtonText("Assign…");
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(6, 4);
        assignButton.setBounds(r.removeFromRight(108).reduced(0, 2));
        r.removeFromRight(8);
        valueLabel.setBounds(r.removeFromRight(juce::jmin(160, r.getWidth() / 3)).reduced(0, 2));
        r.removeFromRight(6);
        nameLabel.setBounds(r);
    }

    static constexpr int preferredHeight() noexcept { return 40; }

private:
    void showAssignMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader("Map hardware");

        menu.addItem(10, "K1");
        menu.addItem(11, "K2");
        menu.addItem(12, "K3");
        menu.addItem(13, "K4");
        menu.addSeparator();
        menu.addItem(14, "Assign button 1");
        menu.addItem(15, "Assign button 2");
        menu.addSeparator();
        menu.addItem(1, "Clear mapping");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&assignButton),
                           [this](const int result)
                           {
                               if (result <= 0)
                                   return;

                               if (result == 1)
                               {
                                   clearMapping();
                                   return;
                               }

                               HardwareControlId hid = HardwareControlId::Knob1;

                               switch (result)
                               {
                                   case 10:
                                       hid = HardwareControlId::Knob1;
                                       break;
                                   case 11:
                                       hid = HardwareControlId::Knob2;
                                       break;
                                   case 12:
                                       hid = HardwareControlId::Knob3;
                                       break;
                                   case 13:
                                       hid = HardwareControlId::Knob4;
                                       break;
                                   case 14:
                                       hid = HardwareControlId::AssignButton1;
                                       break;
                                   case 15:
                                       hid = HardwareControlId::AssignButton2;
                                       break;
                                   default:
                                       return;
                               }

                               applyMapping(hid);
                           });
    }

    void clearMapping()
    {
        if (appContext.parameterMappingManager == nullptr || appContext.sceneManager == nullptr)
            return;

        auto* scene = appContext.sceneManager->getActiveScene();

        if (scene == nullptr)
            return;

        scene->clampActiveVariationIndex();

        const auto& vars = scene->getVariations();
        const int vi = scene->getActiveChainVariationIndex();

        if (!juce::isPositiveAndBelow(vi, static_cast<int>(vars.size())) || vars[static_cast<size_t>(vi)] == nullptr)
            return;

        removeAllMappingsForParameter(*appContext.parameterMappingManager,
                                      scene->getSceneId(),
                                      vars[static_cast<size_t>(vi)]->getVariationId(),
                                      slotIndex,
                                      summary);

        refreshAssignButtonLabel();

        if (owner.onMappingsChanged != nullptr)
            owner.onMappingsChanged();
    }

    void applyMapping(const HardwareControlId hid)
    {
        if (appContext.parameterMappingManager == nullptr)
            return;

        const juce::String display = summary.name.isNotEmpty() ? summary.name : juce::String("Parameter");

        bool toggleForButton = false;
        bool momentaryForButton = true;

        if (isAssignButtonId(hid))
        {
            const juce::AudioProcessorParameter* p = resolveParameterPointer();

            if (parameterLooksBoolean(p))
            {
                toggleForButton = true;
                momentaryForButton = false;
            }
        }

        appContext.parameterMappingManager->assignParameterToHardwareInActiveVariation(hid,
                                                                                       slotIndex,
                                                                                       summary.parameterId,
                                                                                       summary.parameterIndex,
                                                                                       display,
                                                                                       0.0f,
                                                                                       1.0f,
                                                                                       false,
                                                                                       toggleForButton,
                                                                                       momentaryForButton);

        refreshAssignButtonLabel();

        if (owner.onMappingsChanged != nullptr)
            owner.onMappingsChanged();
    }

    const juce::AudioProcessorParameter* resolveParameterPointer() const
    {
        if (appContext.pluginHostManager == nullptr)
            return nullptr;

        auto* chain = appContext.pluginHostManager->getPluginChain();

        if (chain == nullptr)
            return nullptr;

        auto* slot = chain->getSlot(static_cast<size_t>(slotIndex));

        if (slot == nullptr)
            return nullptr;

        auto* instance = slot->getHostedInstance();

        if (instance == nullptr)
            return nullptr;

        auto params = instance->getParameters();

        if (summary.parameterIndex < 0 || summary.parameterIndex >= params.size())
            return nullptr;

        return params[static_cast<size_t>(summary.parameterIndex)];
    }

    PluginInspectorComponent& owner;
    AppContext& appContext;
    int slotIndex { 0 };
    AutomatableParameterSummary summary;

    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::TextButton assignButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterRow)
};

//==============================================================================
PluginInspectorComponent::PluginInspectorComponent(AppContext& context)
    : appContext(context)
{
    setOpaque(true);

    headingLabel.setText("Plugin inspector", juce::dontSendNotification);
    headingLabel.setFont(juce::Font(13.0f));
    headingLabel.setColour(juce::Label::textColourId, mutedColour());
    headingLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headingLabel);

    pluginNameLabel.setJustificationType(juce::Justification::centredLeft);
    pluginNameLabel.setFont(juce::Font(17.0f));
    pluginNameLabel.setColour(juce::Label::textColourId, textColour());
    addAndMakeVisible(pluginNameLabel);

    metaLabel.setJustificationType(juce::Justification::centredLeft);
    metaLabel.setFont(juce::Font(14.0f));
    metaLabel.setColour(juce::Label::textColourId, mutedColour());
    addAndMakeVisible(metaLabel);

    slotLabel.setJustificationType(juce::Justification::centredRight);
    slotLabel.setFont(juce::Font(14.0f));
    slotLabel.setColour(juce::Label::textColourId, accentColour());
    addAndMakeVisible(slotLabel);

    bypassToggle.setColour(juce::ToggleButton::textColourId, textColour());
    bypassToggle.setColour(juce::ToggleButton::tickColourId, accentColour());
    bypassToggle.onClick = [this]()
    {
        if (!juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots))
            return;

        if (appContext.pluginHostManager == nullptr)
            return;

        if (auto* chain = appContext.pluginHostManager->getPluginChain())
            chain->bypassSlot(inspectedSlot, bypassToggle.getToggleState());

        if (onModelChanged != nullptr)
            onModelChanged();
    };
    addAndMakeVisible(bypassToggle);

    removeButton.onClick = [this]()
    {
        if (!juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots))
            return;

        if (appContext.pluginHostManager == nullptr)
            return;

        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->closeFullscreenPluginEditorIfShowingSlot(inspectedSlot);

        if (auto* chain = appContext.pluginHostManager->getPluginChain())
            chain->removePluginFromSlot(inspectedSlot);

        if (onModelChanged != nullptr)
            onModelChanged();
    };

    styleSecondaryButton(removeButton);
    addAndMakeVisible(removeButton);

    openEditorButton.onClick = [this]() { openFullscreenPluginEditor(); };

    styleSecondaryButton(openEditorButton);
    addAndMakeVisible(openEditorButton);

    moveLeftButton.onClick = [this]()
    {
        const int s = inspectedSlot;

        if (!juce::isPositiveAndBelow(s, kPluginChainMaxSlots) || s <= 0)
            return;

        if (appContext.pluginHostManager == nullptr)
            return;

        auto* chain = appContext.pluginHostManager->getPluginChain();

        if (chain == nullptr)
            return;

        if (!chain->moveSlot(s, s - 1))
            return;

        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->closeFullscreenPluginEditor();

        setInspectedSlot(s - 1);

        if (onModelChanged != nullptr)
            onModelChanged();
    };

    moveRightButton.onClick = [this]()
    {
        const int s = inspectedSlot;

        if (!juce::isPositiveAndBelow(s, kPluginChainMaxSlots))
            return;

        if (s >= kPluginChainMaxSlots - 1)
            return;

        if (appContext.pluginHostManager == nullptr)
            return;

        auto* chain = appContext.pluginHostManager->getPluginChain();

        if (chain == nullptr)
            return;

        if (!chain->moveSlot(s, s + 1))
            return;

        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->closeFullscreenPluginEditor();

        setInspectedSlot(s + 1);

        if (onModelChanged != nullptr)
            onModelChanged();
    };

    replacePluginButton.onClick = [this]()
    {
        if (!juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots))
            return;

        if (auto* rack = findParentComponentOfClass<RackViewComponent>())
        {
            rack->selectRackSlot(inspectedSlot);
            rack->showPluginBrowser();
        }
    };

    styleSecondaryButton(moveLeftButton);
    styleSecondaryButton(moveRightButton);
    styleSecondaryButton(replacePluginButton);
    addAndMakeVisible(moveLeftButton);
    addAndMakeVisible(moveRightButton);
    addAndMakeVisible(replacePluginButton);

    emptyHintLabel.setJustificationType(juce::Justification::centredLeft);
    emptyHintLabel.setFont(juce::Font(14.0f));
    emptyHintLabel.setColour(juce::Label::textColourId, mutedColour());
    emptyHintLabel.setText("Select a slot with a loaded plugin to edit parameters and mappings.", juce::dontSendNotification);
    addAndMakeVisible(emptyHintLabel);

    parameterList = std::make_unique<juce::Component>();
    parameterViewport.setViewedComponent(parameterList.get(), false);
    parameterViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(parameterViewport);

    startTimerHz(10);
}

PluginInspectorComponent::~PluginInspectorComponent()
{
    stopTimer();
}

void PluginInspectorComponent::setInspectedSlot(const int slotIndex)
{
    inspectedSlot = slotIndex;
    refreshFromHost();
}

void PluginInspectorComponent::refreshFromHost()
{
    refreshHeader();
    updateEmptyStateVisibility();

    const juce::String sig = computeSlotSignature();

    if (sig != lastSlotSignature)
    {
        lastSlotSignature = sig;
        rebuildParameterRows();
    }

    refreshParameterValueTexts();

    for (auto& row : parameterRows)
        row->refreshAssignButtonLabel();

    resized();
}

juce::String PluginInspectorComponent::computeSlotSignature() const
{
    if (!juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots))
        return {};

    if (appContext.pluginHostManager == nullptr)
        return {};

    auto* chain = appContext.pluginHostManager->getPluginChain();

    if (chain == nullptr)
        return {};

    const SlotInfo info = chain->getSlotInfo(inspectedSlot);

    return juce::String(inspectedSlot) + "|" + info.pluginIdentifier + "|"
           + (info.isEmpty ? juce::String("e") : juce::String("p"));
}

void PluginInspectorComponent::updateEmptyStateVisibility()
{
    const bool hasSlot = juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots);

    bool hasPlugin = false;

    if (hasSlot && appContext.pluginHostManager != nullptr)
    {
        if (auto* chain = appContext.pluginHostManager->getPluginChain())
        {
            const auto info = chain->getSlotInfo(inspectedSlot);
            hasPlugin = !info.isEmpty && !info.isPlaceholder && !info.missingPlugin;
        }
    }

    const bool showInspector = hasSlot && hasPlugin;

    pluginNameLabel.setVisible(showInspector);
    metaLabel.setVisible(showInspector);
    slotLabel.setVisible(showInspector);
    bypassToggle.setVisible(showInspector);
    removeButton.setVisible(showInspector);
    openEditorButton.setVisible(showInspector);
    moveLeftButton.setVisible(showInspector);
    moveRightButton.setVisible(showInspector);
    replacePluginButton.setVisible(showInspector);
    parameterViewport.setVisible(showInspector);

    emptyHintLabel.setVisible(!showInspector);

    if (!hasSlot)
        emptyHintLabel.setText("Select a rack slot to inspect plugins and map parameters.", juce::dontSendNotification);
    else if (!hasPlugin)
        emptyHintLabel.setText("This slot is empty — load a plugin from Add Plugin.", juce::dontSendNotification);
}

void PluginInspectorComponent::refreshHeader()
{
    if (!juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots))
    {
        pluginNameLabel.setText({}, juce::dontSendNotification);
        metaLabel.setText({}, juce::dontSendNotification);
        slotLabel.setText({}, juce::dontSendNotification);
        return;
    }

    slotLabel.setText("Slot " + juce::String(inspectedSlot + 1), juce::dontSendNotification);

    if (appContext.pluginHostManager == nullptr)
        return;

    auto* chain = appContext.pluginHostManager->getPluginChain();

    if (chain == nullptr)
        return;

    const SlotInfo info = chain->getSlotInfo(inspectedSlot);

    juce::String manufacturer;
    juce::String format;

    if (auto* slot = chain->getSlot(static_cast<size_t>(inspectedSlot)))
    {
        if (auto* inst = slot->getHostedInstance())
        {
            const auto& d = inst->getPluginDescription();
            manufacturer = d.manufacturerName;
            format = d.pluginFormatName;
        }
        else
        {
            const auto& d = slot->getPluginDescription();

            if (d.manufacturerName.isNotEmpty())
                manufacturer = d.manufacturerName;

            if (d.pluginFormatName.isNotEmpty())
                format = d.pluginFormatName;
        }
    }

    juce::String nameLine = info.slotDisplayName;

    if (nameLine.isEmpty())
        nameLine = "Plugin";

    pluginNameLabel.setText(nameLine, juce::dontSendNotification);

    juce::String meta = manufacturer.isNotEmpty() ? manufacturer : juce::String("—");

    meta += "   ·   ";
    meta += format.isNotEmpty() ? format : juce::String("—");

    metaLabel.setText(meta, juce::dontSendNotification);

    bypassToggle.setToggleState(info.bypass, juce::dontSendNotification);
}

void PluginInspectorComponent::rebuildParameterRows()
{
    parameterRows.clear();

    if (parameterList != nullptr)
        parameterList->deleteAllChildren();

    if (!juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots))
        return;

    if (appContext.parameterMappingManager == nullptr)
        return;

    const auto params = appContext.parameterMappingManager->getAutomatableParametersForSlot(inspectedSlot);

    for (const auto& p : params)
    {
        auto row = std::make_unique<ParameterRow>(*this, appContext, inspectedSlot, p);
        parameterList->addAndMakeVisible(row.get());
        parameterRows.push_back(std::move(row));
    }

    refreshParameterValueTexts();

    for (auto& row : parameterRows)
        row->refreshAssignButtonLabel();
}

void PluginInspectorComponent::refreshParameterValueTexts()
{
    for (auto& row : parameterRows)
        row->refreshValueText();
}

void PluginInspectorComponent::timerCallback()
{
    refreshParameterValueTexts();
}

void PluginInspectorComponent::paint(juce::Graphics& g)
{
    g.fillAll(panelBg());

    auto outline = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(panelStroke());
    g.drawRoundedRectangle(outline, 8.0f, 1.2f);
}

void PluginInspectorComponent::resized()
{
    auto area = getLocalBounds().reduced(10, 8);

    headingLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(6);

    auto titleRow = area.removeFromTop(26);
    slotLabel.setBounds(titleRow.removeFromRight(80));
    pluginNameLabel.setBounds(titleRow);

    metaLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    auto buttonRow1 = area.removeFromTop(34);
    const int btnW = juce::jmin(130, buttonRow1.getWidth() / 3);

    bypassToggle.setBounds(buttonRow1.removeFromLeft(btnW + 24));
    buttonRow1.removeFromLeft(6);
    openEditorButton.setBounds(buttonRow1.removeFromLeft(juce::jmin(168, btnW + 40)));
    buttonRow1.removeFromLeft(6);
    removeButton.setBounds(buttonRow1.removeFromLeft(btnW + 40));

    auto buttonRow2 = area.removeFromTop(34);
    const int btn2 = juce::jmin(130, buttonRow2.getWidth() / 3);

    moveLeftButton.setBounds(buttonRow2.removeFromLeft(btn2));
    buttonRow2.removeFromLeft(6);
    moveRightButton.setBounds(buttonRow2.removeFromLeft(btn2));
    buttonRow2.removeFromLeft(6);
    replacePluginButton.setBounds(buttonRow2.removeFromLeft(btn2 + 40));

    area.removeFromTop(10);

    if (emptyHintLabel.isVisible())
        emptyHintLabel.setBounds(area.reduced(0, 20));

    if (parameterViewport.isVisible())
        parameterViewport.setBounds(area);

    const int pw = parameterViewport.getWidth() > 0 ? parameterViewport.getWidth() : area.getWidth();

    if (parameterList != nullptr)
    {
        int y = 0;
        const int rowH = ParameterRow::preferredHeight();

        for (auto& row : parameterRows)
        {
            row->setBounds(0, y, pw, rowH);
            y += rowH;
        }

        parameterList->setSize(pw, juce::jmax(y, parameterViewport.getHeight()));
    }
}

void PluginInspectorComponent::openFullscreenPluginEditor()
{
    jassert(juce::MessageManager::getInstanceWithoutCreating() == nullptr
            || juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (!juce::isPositiveAndBelow(inspectedSlot, kPluginChainMaxSlots))
        return;

    if (appContext.pluginHostManager == nullptr)
        return;

    auto* chain = appContext.pluginHostManager->getPluginChain();

    if (chain == nullptr)
        return;

    auto* slot = chain->getSlot(static_cast<size_t>(inspectedSlot));

    if (slot == nullptr)
        return;

    auto* instance = slot->getHostedInstance();

    if (instance == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                 "Plugin editor",
                                                 "No loaded plugin instance in this slot.",
                                                 "OK");
        return;
    }

    if (auto* main = findParentComponentOfClass<MainComponent>())
        main->openFullscreenPluginEditor(inspectedSlot);
}

} // namespace forge7
