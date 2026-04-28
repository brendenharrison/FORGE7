#include "PluginBrowserComponent.h"

#include <vector>

#include "../PluginHost/PluginHostManager.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{
constexpr int kTitleRowHeight = 44;
constexpr int kScanStatusHeight = 40;
constexpr int kSearchHeight = 48;
constexpr int kCategoryStubHeight = 44;
constexpr int kHeaderRowHeight = 30;
constexpr int kRowHeight = 88;
constexpr int kButtonRowHeight = 54;
constexpr int kErrorMinHeight = 52;
constexpr float kPad = 12.0f;

/** Column fractions for name, manufacturer, format, compatibility (must sum to ~1). */
constexpr float kFracName = 0.34f;
constexpr float kFracManufacturer = 0.30f;
constexpr float kFracFormat = 0.20f;
constexpr float kFracCompat = 0.16f;

juce::Colour browserBg() noexcept { return juce::Colour(0xff1e2229); }
juce::Colour browserText() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour browserMuted() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour browserAccent() noexcept { return juce::Colour(0xff4a9eff); }
juce::Colour browserDanger() noexcept { return juce::Colour(0xffff8a80); }

void styleSearchBox(juce::TextEditor& ed)
{
    ed.setFont(juce::Font(18.0f));
    ed.applyFontToAllText(ed.getFont());
    ed.setIndents(14, (kSearchHeight - 18) / 2 + 1);
}

void styleScanButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, browserAccent().withAlpha(0.38f));
    b.setColour(juce::TextButton::textColourOffId, browserText());
    b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void splitColumns(const int innerWidth, int& wName, int& wMfg, int& wFmt, int& wCompat)
{
    wName = juce::roundToInt(static_cast<float>(innerWidth) * kFracName);
    wMfg = juce::roundToInt(static_cast<float>(innerWidth) * kFracManufacturer);
    wFmt = juce::roundToInt(static_cast<float>(innerWidth) * kFracFormat);
    wCompat = innerWidth - wName - wMfg - wFmt;

    if (wCompat < 52)
    {
        const int deficit = 52 - wCompat;
        const int take = juce::jmin(deficit, juce::jmax(0, wName - 80));
        wName -= take;
        wCompat += take;
    }
}
} // namespace

PluginBrowserComponent::PluginBrowserComponent(PluginHostManager& hostManager)
    : pluginHostManager(hostManager)
{
    titleLabel.setText("Plugins", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(22.0f));
    titleLabel.setColour(juce::Label::textColourId, browserText());
    addAndMakeVisible(titleLabel);

    scanPluginsButton.onClick = [this]() { onScanPluginsClicked(); };
    styleScanButton(scanPluginsButton);
    addAndMakeVisible(scanPluginsButton);

    scanStatusLabel.setText("No scan run yet", juce::dontSendNotification);
    scanStatusLabel.setJustificationType(juce::Justification::centredLeft);
    scanStatusLabel.setFont(juce::Font(14.0f));
    scanStatusLabel.setColour(juce::Label::textColourId, browserMuted());
    scanStatusLabel.setMinimumHorizontalScale(0.72f);
    addAndMakeVisible(scanStatusLabel);

    filterEditor.setTextToShowWhenEmpty("Search plugins…", browserMuted());
    filterEditor.setColour(juce::TextEditor::backgroundColourId, browserBg().brighter(0.08f));
    filterEditor.setColour(juce::TextEditor::textColourId, browserText());
    filterEditor.setColour(juce::TextEditor::outlineColourId, browserAccent().withAlpha(0.35f));
    filterEditor.setColour(juce::TextEditor::focusedOutlineColourId, browserAccent().withAlpha(0.65f));
    styleSearchBox(filterEditor);
    filterEditor.onTextChange = [this]()
    {
        filterAndRefresh();
    };
    filterEditor.onReturnKey = [this]()
    {
        if (filteredRows.size() > 0)
        {
            listBox.selectRow(0);
            confirmRow(0);
        }
    };
    addAndMakeVisible(filterEditor);

    categoryPlaceholderLabel.setText("Categories — filters coming soon", juce::dontSendNotification);
    categoryPlaceholderLabel.setJustificationType(juce::Justification::centred);
    categoryPlaceholderLabel.setFont(juce::Font(16.0f));
    categoryPlaceholderLabel.setColour(juce::Label::textColourId, browserMuted());
    categoryPlaceholderLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    categoryPlaceholderLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(categoryPlaceholderLabel);

    auto setupHeader = [this](juce::Label& lab, const juce::String& t)
    {
        lab.setText(t, juce::dontSendNotification);
        lab.setJustificationType(juce::Justification::centredLeft);
        lab.setFont(juce::Font(14.0f));
        lab.setColour(juce::Label::textColourId, browserMuted());
        lab.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(lab);
    };

    setupHeader(headerName, "Plugin");
    setupHeader(headerManufacturer, "Manufacturer");
    setupHeader(headerFormat, "Format");
    setupHeader(headerCompatibility, "Status");

    listBox.setRowHeight(kRowHeight);
    listBox.setColour(juce::ListBox::backgroundColourId, browserBg().brighter(0.04f));
    listBox.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    listBox.setMultipleSelectionEnabled(false);
    addAndMakeVisible(listBox);

    loadErrorLabel.setJustificationType(juce::Justification::centredLeft);
    loadErrorLabel.setFont(juce::Font(15.0f));
    loadErrorLabel.setColour(juce::Label::textColourId, browserDanger());
    loadErrorLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff3a2020));
    loadErrorLabel.setBorderSize(juce::BorderSize<int>(10, 14, 10, 14));
    loadErrorLabel.setMinimumHorizontalScale(0.75f);
    loadErrorLabel.setVisible(false);
    addAndMakeVisible(loadErrorLabel);

    addButton.onClick = [this]()
    {
        const int visRow = listBox.getSelectedRow();

        if (juce::isPositiveAndBelow(visRow, filteredRows.size()))
            confirmRow(visRow);
    };

    addButton.setColour(juce::TextButton::buttonColourId, browserAccent().withAlpha(0.42f));
    addButton.setColour(juce::TextButton::textColourOffId, browserText());
    addAndMakeVisible(addButton);

    cancelButton.onClick = [this]()
    {
        if (onDismiss != nullptr)
            onDismiss();
    };

    cancelButton.setColour(juce::TextButton::buttonColourId, browserBg().brighter(0.14f));
    cancelButton.setColour(juce::TextButton::textColourOffId, browserText());
    addAndMakeVisible(cancelButton);

    rebuildList();
}

PluginBrowserComponent::~PluginBrowserComponent() = default;

void PluginBrowserComponent::setLoadErrorMessage(const juce::String& message)
{
    loadErrorVisible = message.isNotEmpty();
    loadErrorLabel.setText(message, juce::dontSendNotification);
    loadErrorLabel.setVisible(loadErrorVisible);
    resized();
}

void PluginBrowserComponent::clearLoadError() noexcept
{
    loadErrorVisible = false;
    loadErrorLabel.setText({}, juce::dontSendNotification);
    loadErrorLabel.setVisible(false);
}

bool PluginBrowserComponent::isVst3Description(const juce::PluginDescription& d) noexcept
{
    return d.pluginFormatName.containsIgnoreCase("VST3");
}

void PluginBrowserComponent::rebuildList()
{
    clearLoadError();

    const auto raw = pluginHostManager.getAvailablePluginDescriptions();
    allDescriptions.clear();

    for (int i = 0; i < raw.size(); ++i)
    {
        const auto& d = raw.getReference(i);

        if (isVst3Description(d))
            allDescriptions.add(d);
    }

    filterAndRefresh();
}

void PluginBrowserComponent::updateStatusForEmptyVst3List()
{
    if (!hasCompletedPluginScan || !allDescriptions.isEmpty())
        return;

    scanStatusLabel.setText(
        "No VST3 plugins found. Confirm plugins exist in /Library/Audio/Plug-Ins/VST3 or "
        "~/Library/Audio/Plug-Ins/VST3.",
        juce::dontSendNotification);
}

void PluginBrowserComponent::onScanPluginsClicked()
{
    if (scanInProgress)
        return;

    scanInProgress = true;
    scanPluginsButton.setEnabled(false);
    scanStatusLabel.setColour(juce::Label::textColourId, browserText());
    scanStatusLabel.setText("Scanning plugins…", juce::dontSendNotification);

    Logger::info("FORGE7: Plugin Browser — Scan Plugins clicked; registering VST3 folders and starting async scan");

    pluginHostManager.addStandardPlatformScanDirectories();

    pluginHostManager.addPluginScanDirectory(juce::File("/Library/Audio/Plug-Ins/VST3"));
    Logger::info("FORGE7: Plugin Browser — ensured scan folder — /Library/Audio/Plug-Ins/VST3");

    const auto userVst3 =
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/VST3");
    pluginHostManager.addPluginScanDirectory(userVst3);
    Logger::info("FORGE7: Plugin Browser — ensured scan folder — " + userVst3.getFullPathName());

    pluginHostManager.scanAllPluginsAsync([this](const int added) { onScanFinished(added); });
}

void PluginBrowserComponent::onScanFinished(const int numAdded)
{
    scanInProgress = false;
    scanPluginsButton.setEnabled(true);
    hasCompletedPluginScan = true;

    const int totalKnown = pluginHostManager.getKnownPluginDescriptionCount();

    rebuildList();

    if (allDescriptions.isEmpty())
    {
        updateStatusForEmptyVst3List();
    }
    else if (numAdded > 0)
    {
        scanStatusLabel.setText("Scan complete: added " + juce::String(numAdded) + " plugins — total in list "
                                    + juce::String(totalKnown),
                                juce::dontSendNotification);
    }
    else
    {
        scanStatusLabel.setText("Scan complete: found " + juce::String(allDescriptions.size()) + " VST3 plugins (list "
                                    + juce::String(totalKnown) + " descriptions total)",
                                juce::dontSendNotification);
    }

    if (filteredRows.size() > 0)
    {
        listBox.selectRow(0);
        listBox.scrollToEnsureRowIsOnscreen(0);
    }

    syncAddButtonEnabled();
    listBox.repaint();

    Logger::info("FORGE7: Plugin Browser — scan UI refresh complete; VST3 rows shown: "
                 + juce::String(allDescriptions.size()));
}

void PluginBrowserComponent::filterAndRefresh()
{
    filteredRows.clear();

    const juce::String q = filterEditor.getText().trim().toLowerCase();

    for (int i = 0; i < allDescriptions.size(); ++i)
    {
        const auto& d = allDescriptions.getReference(i);
        const juce::String compatPlaceholder = "status";
        const juce::String hay =
            (d.name + " " + d.manufacturerName + " " + d.pluginFormatName + " " + compatPlaceholder).toLowerCase();

        if (q.isEmpty() || hay.contains(q))
            filteredRows.add(i);
    }

    listBox.updateContent();
    listBox.deselectAllRows();
    syncAddButtonEnabled();
    listBox.repaint();
}

void PluginBrowserComponent::syncAddButtonEnabled()
{
    const bool hasSelection =
        juce::isPositiveAndBelow(listBox.getSelectedRow(), filteredRows.size());

    addButton.setEnabled(hasSelection && filteredRows.size() > 0);
}

void PluginBrowserComponent::layoutColumnHeaders(juce::Rectangle<int> headerArea)
{
    const int innerW = juce::jmax(0, headerArea.getWidth() - static_cast<int>(kPad * 2));
    int wName = 0, wMfg = 0, wFmt = 0, wCompat = 0;
    splitColumns(innerW, wName, wMfg, wFmt, wCompat);

    int x = headerArea.getX() + static_cast<int>(kPad);
    const int y = headerArea.getY();
    const int h = headerArea.getHeight();

    headerName.setBounds(x, y, wName, h);
    x += wName;
    headerManufacturer.setBounds(x, y, wMfg, h);
    x += wMfg;
    headerFormat.setBounds(x, y, wFmt, h);
    x += wFmt;
    headerCompatibility.setBounds(x, y, wCompat, h);
}

void PluginBrowserComponent::paint(juce::Graphics& g)
{
    g.fillAll(browserBg());

    auto catBounds = categoryPlaceholderLabel.getBounds().toFloat();
    g.setColour(browserBg().brighter(0.06f));
    g.fillRoundedRectangle(catBounds, 8.0f);
    g.setColour(browserMuted().withAlpha(0.35f));
    g.drawRoundedRectangle(catBounds, 8.0f, 1.0f);
}

void PluginBrowserComponent::resized()
{
    auto r = getLocalBounds().reduced(10, 12);

    auto titleRow = r.removeFromTop(kTitleRowHeight);
    scanPluginsButton.setBounds(titleRow.removeFromRight(148).reduced(0, 4));
    titleRow.removeFromRight(8);
    titleLabel.setBounds(titleRow);

    r.removeFromTop(6);

    scanStatusLabel.setBounds(r.removeFromTop(kScanStatusHeight));
    r.removeFromTop(8);

    filterEditor.setBounds(r.removeFromTop(kSearchHeight));
    r.removeFromTop(10);

    categoryPlaceholderLabel.setBounds(r.removeFromTop(kCategoryStubHeight));
    r.removeFromTop(10);

    auto headerSlice = r.removeFromTop(kHeaderRowHeight);
    layoutColumnHeaders(headerSlice);
    r.removeFromTop(6);

    auto bottom = r.removeFromBottom(kButtonRowHeight);
    cancelButton.setBounds(bottom.removeFromRight(140).reduced(0, 6));
    bottom.removeFromRight(10);
    addButton.setBounds(bottom.removeFromRight(160).reduced(8, 6));

    if (loadErrorVisible)
        loadErrorLabel.setBounds(r.removeFromBottom(kErrorMinHeight));
    else
        loadErrorLabel.setBounds({});

    r.removeFromBottom(loadErrorVisible ? 10 : 0);

    listBox.setBounds(r);
}

int PluginBrowserComponent::getNumRows()
{
    return filteredRows.size();
}

void PluginBrowserComponent::paintListBoxItem(const int rowNumber,
                                              juce::Graphics& g,
                                              const int width,
                                              const int height,
                                              const bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow(rowNumber, filteredRows.size()))
        return;

    const int idx = filteredRows[rowNumber];
    const auto& d = allDescriptions.getReference(idx);

    const auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(5.0f, 4.0f);

    if (rowIsSelected)
    {
        g.setColour(browserAccent().withAlpha(0.38f));
        g.fillRoundedRectangle(bounds, 8.0f);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.045f));
        g.fillRoundedRectangle(bounds, 8.0f);
    }

    const int pad = static_cast<int>(kPad);
    const int innerW = juce::jmax(0, width - pad * 2);
    int wName = 0, wMfg = 0, wFmt = 0, wCompat = 0;
    splitColumns(innerW, wName, wMfg, wFmt, wCompat);

    int x = pad;
    const int yName = 14;
    const int lineH = 22;

    g.setColour(browserText());
    g.setFont(juce::Font(18.0f));
    g.drawText(d.name,
               x,
               yName,
               wName,
               lineH,
               juce::Justification::centredLeft,
               true);

    g.setColour(browserMuted());
    g.setFont(juce::Font(14.5f));
    g.drawText(d.manufacturerName.isNotEmpty() ? d.manufacturerName : "—",
               x + wName,
               yName,
               wMfg,
               lineH,
               juce::Justification::centredLeft,
               true);

    g.drawText(d.pluginFormatName.isNotEmpty() ? d.pluginFormatName : "—",
               x + wName + wMfg,
               yName,
               wFmt,
               lineH,
               juce::Justification::centredLeft,
               true);

    const juce::String compatLine = "\u2014";
    g.drawText(compatLine,
               x + wName + wMfg + wFmt,
               yName,
               wCompat,
               lineH,
               juce::Justification::centredLeft,
               true);

}

void PluginBrowserComponent::selectedRowsChanged(int lastRowSelected)
{
    juce::ignoreUnused(lastRowSelected);
    syncAddButtonEnabled();
}

void PluginBrowserComponent::listBoxItemClicked(const int row, const juce::MouseEvent& e)
{
    juce::ignoreUnused(row, e);
}

void PluginBrowserComponent::listBoxItemDoubleClicked(const int row, const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (juce::isPositiveAndBelow(row, filteredRows.size()))
        confirmRow(row);
}

void PluginBrowserComponent::confirmRow(const int visibleRowIndex)
{
    if (! juce::isPositiveAndBelow(visibleRowIndex, filteredRows.size()))
        return;

    const int descriptionIndex = filteredRows[visibleRowIndex];

    if (! juce::isPositiveAndBelow(descriptionIndex, allDescriptions.size()))
        return;

    const juce::PluginDescription desc(allDescriptions.getReference(descriptionIndex));

    if (onPluginChosen != nullptr)
        onPluginChosen(desc);
}

void PluginBrowserComponent::ensureDefaultListSelectionForEncoder()
{
    if (filteredRows.isEmpty())
        return;

    if (listBox.getSelectedRow() < 0)
        listBox.selectRow(0);
}

void PluginBrowserComponent::encoderNudgeListSelection(const int deltaSteps)
{
    if (filteredRows.isEmpty())
        return;

    const int n = filteredRows.size();
    int r = listBox.getSelectedRow();

    if (r < 0)
        r = 0;

    r = juce::jlimit(0, n - 1, r + deltaSteps);
    listBox.selectRow(r);
    listBox.scrollToEnsureRowIsOnscreen(r);
}

std::vector<EncoderFocusItem> PluginBrowserComponent::buildEncoderFocusItems()
{
    std::vector<EncoderFocusItem> items;

    items.push_back({ &scanPluginsButton,
                      [this]()
                      {
                          if (scanPluginsButton.isEnabled())
                              scanPluginsButton.triggerClick();
                      },
                      {} });

    items.push_back({ &filterEditor,
                      [this]()
                      {
                          filterEditor.grabKeyboardFocus();
                      },
                      {} });

    items.push_back({ &listBox,
                      [this]()
                      {
                          const int r = listBox.getSelectedRow();

                          if (juce::isPositiveAndBelow(r, filteredRows.size()))
                              confirmRow(r);
                      },
                      [this](const int delta)
                      {
                          encoderNudgeListSelection(delta);
                      } });

    items.push_back({ &addButton,
                      [this]() { addButton.triggerClick(); },
                      {} });

    items.push_back({ &cancelButton,
                      [this]() { cancelButton.triggerClick(); },
                      {} });

    return items;
}

} // namespace forge7
