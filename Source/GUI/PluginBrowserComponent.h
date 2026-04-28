#pragma once

#include <functional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../Controls/EncoderFocusTypes.h"

namespace forge7
{

class PluginHostManager;

/** Touch-friendly plugin picker for a rack slot: scanned plugins, search, multi-column list.

    Does not open the plugin's editor - only selection for host instantiation. */
class PluginBrowserComponent final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PluginBrowserComponent(PluginHostManager& hostManager);
    ~PluginBrowserComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** If set, invoked when user confirms a plugin (Add or double-tap row). Host should load into slot. */
    std::function<void(const juce::PluginDescription&)> onPluginChosen;

    /** If set, invoked when user cancels without adding. */
    std::function<void()> onDismiss;

    void rebuildList();

    /** Shows a prominent error under the list (e.g. load failure). Empty string hides the banner. */
    void setLoadErrorMessage(const juce::String& message);

    void clearLoadError() noexcept;

    /** Ordered: search, list (rotate moves selection), Add, Cancel - for EncoderNavigator. */
    std::vector<EncoderFocusItem> buildEncoderFocusItems();

    void ensureDefaultListSelectionForEncoder();

    void encoderNudgeListSelection(int deltaSteps);

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber,
                          juce::Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    void filterAndRefresh();
    void confirmRow(int row);
    void syncAddButtonEnabled();
    void layoutColumnHeaders(juce::Rectangle<int> headerArea);
    void onScanPluginsClicked();
    void onScanFinished(int numAdded);
    void updateStatusForEmptyVst3List();
    static bool isVst3Description(const juce::PluginDescription& d) noexcept;

    PluginHostManager& pluginHostManager;

    juce::Label titleLabel;
    juce::TextButton scanPluginsButton { "Scan Plugins" };
    juce::Label scanStatusLabel;
    juce::TextEditor filterEditor;

    /** Non-interactive placeholder for future category filtering. */
    juce::Label categoryPlaceholderLabel;

    juce::Label headerName;
    juce::Label headerManufacturer;
    juce::Label headerFormat;
    juce::Label headerCompatibility;

    juce::ListBox listBox { {}, this };

    juce::Label loadErrorLabel;

    juce::TextButton addButton { "Add" };
    juce::TextButton cancelButton { "Cancel" };

    juce::Array<juce::PluginDescription> allDescriptions;
    juce::Array<int> filteredRows;

    bool loadErrorVisible { false };
    bool scanInProgress { false };
    bool hasCompletedPluginScan { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBrowserComponent)
};

} // namespace forge7
