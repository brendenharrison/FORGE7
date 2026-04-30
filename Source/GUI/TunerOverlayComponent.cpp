#include "TunerOverlayComponent.h"

#include <cmath>

#include "../App/AppContext.h"
#include "../App/AppConfig.h"
#include "../Audio/AudioEngine.h"
#include "../Controls/EncoderFocusTypes.h"
#include "../Controls/EncoderNavigator.h"

namespace forge7
{
namespace
{
juce::Colour bg() noexcept { return juce::Colour(0xff0a0c0f); }
juce::Colour text() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour accent() noexcept { return juce::Colour(0xff4a9eff); }
juce::Colour good() noexcept { return juce::Colour(0xff4caf50); }
juce::Colour warn() noexcept { return juce::Colour(0xffffc44d); }
juce::Colour bad() noexcept { return juce::Colour(0xffff5252); }
juce::Colour muted() noexcept { return juce::Colour(0xff8a9099); }
} // namespace

TunerOverlayComponent::TunerOverlayComponent(AppContext& context, std::function<void()> closeHandler)
    : appContext(context)
    , onRequestClose(std::move(closeHandler))
{
    setOpaque(true);
    setInterceptsMouseClicks(true, true);

    titleLabel.setText("TUNER", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, text());
    addAndMakeVisible(titleLabel);

    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b2028));
    closeButton.setColour(juce::TextButton::textColourOffId, text());
    closeButton.onClick = [this]()
    {
        if (onRequestClose != nullptr)
            onRequestClose();
    };
    addAndMakeVisible(closeButton);

    auto styleBig = [this](juce::Label& l)
    {
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, text());
    };

    noteNameLabel.setFont(juce::Font(110.0f, juce::Font::bold));
    styleBig(noteNameLabel);
    noteNameLabel.setText("-", juce::dontSendNotification);
    addAndMakeVisible(noteNameLabel);

    octaveLabel.setFont(juce::Font(34.0f));
    styleBig(octaveLabel);
    octaveLabel.setText("", juce::dontSendNotification);
    addAndMakeVisible(octaveLabel);

    centsLabel.setFont(juce::Font(30.0f));
    styleBig(centsLabel);
    centsLabel.setText("", juce::dontSendNotification);
    addAndMakeVisible(centsLabel);

    statusLabel.setFont(juce::Font(34.0f, juce::Font::bold));
    styleBig(statusLabel);
    statusLabel.setText("No signal", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    footnoteLabel.setFont(juce::Font(15.0f));
    footnoteLabel.setJustificationType(juce::Justification::centred);
    footnoteLabel.setColour(juce::Label::textColourId, muted());
    footnoteLabel.setText("Chain - + Chain + toggles tuner | Encoder long press closes", juce::dontSendNotification);
    addAndMakeVisible(footnoteLabel);

    analysisScratch.resize(4096, 0.0f);

    startTimerHz(25);
}

TunerOverlayComponent::~TunerOverlayComponent()
{
    stopTimer();
}

void TunerOverlayComponent::visibilityChanged()
{
    juce::Component::visibilityChanged();

    if (appContext.encoderNavigator == nullptr)
        return;

    if (isShowing())
    {
        std::vector<EncoderFocusItem> items;
        items.push_back({ &closeButton,
                          [this]()
                          {
                              if (onRequestClose != nullptr)
                                  onRequestClose();
                          },
                          {} });
        appContext.encoderNavigator->setModalFocusChain(std::move(items));
    }
    else
    {
        appContext.encoderNavigator->clearModalFocusChain();
    }
}

void TunerOverlayComponent::timerCallback()
{
    if (appContext.audioEngine == nullptr)
        return;

    const int n = appContext.audioEngine->copyTunerMonoSnapshot(analysisScratch.data(),
                                                                static_cast<int>(analysisScratch.size()));

    if (n <= 0)
        return;

    const double sr = appContext.audioEngine->getCurrentSampleRate();
    lastState = TunerPitchAnalyzer::analyze(analysisScratch.data(), n, sr);

    if (!lastState.signalPresent)
    {
        noteNameLabel.setText("-", juce::dontSendNotification);
        octaveLabel.setText("", juce::dontSendNotification);
        centsLabel.setText("", juce::dontSendNotification);
        statusLabel.setText("No signal", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, bad());
    }
    else
    {
        noteNameLabel.setText(lastState.noteName, juce::dontSendNotification);
        octaveLabel.setText(juce::String(lastState.octave), juce::dontSendNotification);

        const int centsRounded = static_cast<int>(std::lround(static_cast<double>(lastState.centsOffset)));
        centsLabel.setText(juce::String(centsRounded) + " cents", juce::dontSendNotification);

        const float ac = std::abs(lastState.centsOffset);
        juce::String status;

        if (ac <= 3.0f)
        {
            status = "In tune";
            statusLabel.setColour(juce::Label::textColourId, good());
        }
        else if (ac <= 10.0f)
        {
            status = lastState.centsOffset < 0.0f ? "Flat (close)" : "Sharp (close)";
            statusLabel.setColour(juce::Label::textColourId, warn());
        }
        else
        {
            status = lastState.centsOffset < 0.0f ? "Flat" : "Sharp";
            statusLabel.setColour(juce::Label::textColourId, bad());
        }

        statusLabel.setText(status, juce::dontSendNotification);
    }

    juce::String foot = "Chain - + Chain + toggles tuner | Encoder long press closes | Input ";
    foot += (appContext.audioEngine != nullptr && appContext.audioEngine->getTunerMutesOutput()) ? "Muted" : "Thru";
    footnoteLabel.setText(foot, juce::dontSendNotification);

    repaint();
}

void TunerOverlayComponent::paint(juce::Graphics& g)
{
    g.fillAll(bg());

    if (needleMeterArea.isEmpty())
        return;

    auto block = needleMeterArea.toFloat().reduced(6.0f, 4.0f);
    auto levelZone = block.removeFromBottom(12.0f);
    auto needleBand = block;

    g.setColour(juce::Colour(0xff1e252e));
    g.fillRoundedRectangle(needleBand, 8.0f);
    g.setColour(juce::Colour(0xff2a3340));
    g.drawRoundedRectangle(needleBand, 8.0f, 2.0f);

    const float cx = needleBand.getCentreX();
    const float wFull = needleMeterArea.toFloat().getWidth();
    const float inTuneHalf = wFull * 0.06f;
    g.setColour(good().withAlpha(0.35f));
    g.fillRoundedRectangle(juce::Rectangle<float>(cx - inTuneHalf, needleBand.getY(), inTuneHalf * 2.0f, needleBand.getHeight()),
                           6.0f);

    auto labelBand = needleBand;
    const float w = needleBand.getWidth();

    g.setColour(muted());
    g.setFont(juce::Font(14.0f));
    g.drawText("FLAT", labelBand.removeFromLeft(w * 0.15f), juce::Justification::centredLeft, false);
    g.drawText("SHARP", labelBand.removeFromRight(w * 0.15f), juce::Justification::centredRight, false);

    float centsForNeedle = lastState.signalPresent ? lastState.centsOffset : 0.0f;
    centsForNeedle = juce::jlimit(-50.0f, 50.0f, centsForNeedle);
    const float t = (centsForNeedle + 50.0f) / 100.0f;
    const float nx = labelBand.getX() + t * labelBand.getWidth();

    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(
        juce::Rectangle<float>(4.0f, labelBand.getHeight() * 0.88f).withCentre({ nx, labelBand.getCentreY() }),
        2.0f);

    g.setColour(juce::Colour(0xff1e252e));
    g.fillRoundedRectangle(levelZone, 4.0f);
    const float lvl = juce::jlimit(0.0f, 1.0f, lastState.inputLevel);
    g.setColour(accent());
    g.fillRoundedRectangle(levelZone.withWidth(levelZone.getWidth() * lvl), 4.0f);
}

void TunerOverlayComponent::resized()
{
    auto r = getLocalBounds().reduced(16);

    auto top = r.removeFromTop(44);
    titleLabel.setBounds(top.removeFromLeft(juce::jmin(200, top.getWidth() / 2)));
    closeButton.setBounds(top.removeFromRight(120).reduced(0, 4));

    r.removeFromTop(8);

    noteNameLabel.setBounds(r.removeFromTop(120));
    octaveLabel.setBounds(r.removeFromTop(40));
    centsLabel.setBounds(r.removeFromTop(40));
    r.removeFromTop(8);

    needleMeterArea = r.removeFromTop(88);
    r.removeFromTop(10);

    statusLabel.setBounds(r.removeFromTop(48));
    r.removeFromTop(6);
    footnoteLabel.setBounds(r.removeFromTop(44));
}

} // namespace forge7
