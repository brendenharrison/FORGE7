#include "TunerOverlayComponent.h"

#include <algorithm>
#include <cmath>

#include "../App/AppContext.h"
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

constexpr float kSmoothAlpha = 0.42f;
constexpr float kSmoothCatchUp = 0.55f;
constexpr float kLargeJumpCents = 18.0f;
constexpr float kDecayNoSignal = 0.16f;
constexpr float kConfidenceFloor = 0.12f;
constexpr float kClipThreshold = 0.98f;
constexpr uint32_t kClipHoldMs = 1200;
constexpr int kFramesToLockNote = 2;

static const char* noteLetterFromMidi(int midi) noexcept
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int idx = (midi % 12 + 12) % 12;
    return names[idx];
}

/** Matches TunerPitchAnalyzer inputLevel scaling (RMS * 4, clamped). */
float monoInputLevelForMeter(const float* samples, int numSamples) noexcept
{
    if (samples == nullptr || numSamples <= 0)
        return 0.0f;

    double acc = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const double s = static_cast<double>(samples[i]);
        acc += s * s;
    }

    const float rms = static_cast<float>(std::sqrt(acc / static_cast<double>(numSamples)));
    return juce::jlimit(0.0f, 1.0f, rms * 4.0f);
}

void applyExponentialCentsSmoothing(float& displayedCents,
                                    float targetCents,
                                    bool& snapNext) noexcept
{
    if (snapNext)
    {
        displayedCents = targetCents;
        snapNext = false;
        return;
    }

    float alpha = kSmoothAlpha;

    if (std::abs(targetCents - displayedCents) > kLargeJumpCents)
        alpha = kSmoothCatchUp;

    displayedCents += (targetCents - displayedCents) * alpha;
}
} // namespace

int TunerOverlayComponent::midiNoteFromHz(const float hz) noexcept
{
    if (hz <= 1.0f || !std::isfinite(hz))
        return -1;

    const float midiFloat = 69.0f + 12.0f * std::log2(hz / 440.0f);
    return static_cast<int>(std::lround(static_cast<double>(midiFloat)));
}

void TunerOverlayComponent::resetSmoothingState() noexcept
{
    displayedCents = 0.0f;
    stableDisplayedMidi = -1;
    pendingMidi = -1;
    pendingFrames = 0;
    snapDisplayCentsNext = false;
    centsWriteIdx = 0;
    centsRingCount = 0;
    centsRing.fill(0.0f);
    weakSignalHold = false;
    clipHoldUntilMs = 0;
    needleDrawActive = false;
    analysisTickCounter = 0;
    lastPitchTargetCents = 0.0f;
}

float TunerOverlayComponent::medianRecentCents() const noexcept
{
    if (centsRingCount <= 0)
        return 0.0f;

    float tmp[5];
    const int n = juce::jmin(5, centsRingCount);

    for (int i = 0; i < n; ++i)
        tmp[i] = centsRing[static_cast<size_t>((centsWriteIdx - 1 - i + 500) % 5)];

    std::sort(tmp, tmp + n);

    if ((n % 2) == 1)
        return tmp[n / 2];

    return 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
}

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

    startTimerHz(50);
}

TunerOverlayComponent::~TunerOverlayComponent()
{
    stopTimer();
}

void TunerOverlayComponent::visibilityChanged()
{
    juce::Component::visibilityChanged();

    if (isShowing())
        resetSmoothingState();

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

    const double sr = appContext.audioEngine->getCurrentSampleRate();
    const float rawPeak = appContext.audioEngine->getLastInputPeakRaw();

    const uint32_t nowMs = juce::Time::getApproximateMillisecondCounter();

    if (rawPeak >= kClipThreshold)
        clipHoldUntilMs = nowMs + kClipHoldMs;

    if (n <= 0)
    {
        lastState = {};
        needleDrawActive = false;
        weakSignalHold = false;
        lastPitchTargetCents = 0.0f;
        displayedCents *= (1.0f - kDecayNoSignal);
        noteNameLabel.setText("-", juce::dontSendNotification);
        octaveLabel.setText("", juce::dontSendNotification);
        centsLabel.setText("", juce::dontSendNotification);
        statusLabel.setText("No signal", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, bad());
        repaint();
        return;
    }

    const float quickLevel = monoInputLevelForMeter(analysisScratch.data(), n);
    ++analysisTickCounter;
    const bool doPitchAnalysis = (analysisTickCounter % 2) == 1;

    if (doPitchAnalysis)
    {
        lastState = TunerPitchAnalyzer::analyze(analysisScratch.data(), n, sr);
        lastState.inputLevel = quickLevel;

        if (!lastState.signalPresent)
        {
            needleDrawActive = false;
            weakSignalHold = false;
            lastPitchTargetCents = 0.0f;
            displayedCents *= (1.0f - kDecayNoSignal);
            noteNameLabel.setText("-", juce::dontSendNotification);
            octaveLabel.setText("", juce::dontSendNotification);
            centsLabel.setText("", juce::dontSendNotification);
            statusLabel.setText("No signal", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, bad());
            juce::String foot = "Chain - + Chain + toggles tuner | Encoder long press closes | Input ";
            foot += (appContext.audioEngine != nullptr && appContext.audioEngine->getTunerMutesOutput()) ? "Muted" : "Thru";
            footnoteLabel.setText(foot, juce::dontSendNotification);
            repaint();
            return;
        }

        weakSignalHold = lastState.confidence < kConfidenceFloor;

        const int candidateMidi = midiNoteFromHz(lastState.frequencyHz);

        if (candidateMidi != pendingMidi)
        {
            pendingMidi = candidateMidi;
            pendingFrames = 1;
        }
        else
        {
            ++pendingFrames;
        }

        const bool lockedNote = (pendingFrames >= kFramesToLockNote && candidateMidi == pendingMidi);

        if (lockedNote && stableDisplayedMidi != pendingMidi)
        {
            stableDisplayedMidi = pendingMidi;
            centsRingCount = 0;
            centsWriteIdx = 0;
            centsRing.fill(0.0f);
            snapDisplayCentsNext = true;
        }

        if (lockedNote && stableDisplayedMidi >= 0 && lastState.confidence >= kConfidenceFloor && !weakSignalHold)
        {
            centsRing[static_cast<size_t>(centsWriteIdx % 5)] = lastState.centsOffset;
            ++centsWriteIdx;
            centsRingCount = juce::jmin(5, centsRingCount + 1);
        }

        float targetCents = lastState.centsOffset;

        if (weakSignalHold)
            lastPitchTargetCents = displayedCents;
        else
        {
            if (centsRingCount >= 2)
                targetCents = medianRecentCents();

            lastPitchTargetCents = targetCents;
        }
    }
    else
    {
        lastState.inputLevel = quickLevel;
    }

    applyExponentialCentsSmoothing(displayedCents, lastPitchTargetCents, snapDisplayCentsNext);

    if (!lastState.signalPresent)
    {
        needleDrawActive = false;
        displayedCents *= (1.0f - kDecayNoSignal);
        noteNameLabel.setText("-", juce::dontSendNotification);
        octaveLabel.setText("", juce::dontSendNotification);
        centsLabel.setText("", juce::dontSendNotification);
        statusLabel.setText("No signal", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, bad());
        juce::String foot = "Chain - + Chain + toggles tuner | Encoder long press closes | Input ";
        foot += (appContext.audioEngine != nullptr && appContext.audioEngine->getTunerMutesOutput()) ? "Muted" : "Thru";
        footnoteLabel.setText(foot, juce::dontSendNotification);
        repaint();
        return;
    }

    if (weakSignalHold)
    {
        noteNameLabel.setColour(juce::Label::textColourId, muted());
        octaveLabel.setColour(juce::Label::textColourId, muted());
        centsLabel.setColour(juce::Label::textColourId, muted());
        statusLabel.setText("Listening...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, muted());
    }
    else
    {
        const int hzMidi = midiNoteFromHz(lastState.frequencyHz);
        const bool lockedNote = (pendingFrames >= kFramesToLockNote) && (hzMidi == pendingMidi);

        if (stableDisplayedMidi >= 0 && lockedNote)
        {
            noteNameLabel.setColour(juce::Label::textColourId, text());
            octaveLabel.setColour(juce::Label::textColourId, text());
            centsLabel.setColour(juce::Label::textColourId, text());

            noteNameLabel.setText(noteLetterFromMidi(stableDisplayedMidi), juce::dontSendNotification);
            octaveLabel.setText(juce::String((stableDisplayedMidi / 12) - 1), juce::dontSendNotification);

            const int centsRounded = static_cast<int>(std::lround(static_cast<double>(displayedCents)));
            centsLabel.setText(juce::String(centsRounded) + " cents", juce::dontSendNotification);

            const float ac = std::abs(displayedCents);
            juce::String status;

            if (ac <= 3.0f)
            {
                status = "In tune";
                statusLabel.setColour(juce::Label::textColourId, good());
            }
            else if (ac <= 10.0f)
            {
                status = displayedCents < 0.0f ? "Flat (close)" : "Sharp (close)";
                statusLabel.setColour(juce::Label::textColourId, warn());
            }
            else
            {
                status = displayedCents < 0.0f ? "Flat" : "Sharp";
                statusLabel.setColour(juce::Label::textColourId, bad());
            }

            statusLabel.setText(status, juce::dontSendNotification);
        }
        else
        {
            noteNameLabel.setText("-", juce::dontSendNotification);
            octaveLabel.setText("", juce::dontSendNotification);
            centsLabel.setText("", juce::dontSendNotification);
            statusLabel.setText("Listening...", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, muted());
        }
    }

    juce::String foot = "Chain - + Chain + toggles tuner | Encoder long press closes | Input ";
    foot += (appContext.audioEngine != nullptr && appContext.audioEngine->getTunerMutesOutput()) ? "Muted" : "Thru";
    footnoteLabel.setText(foot, juce::dontSendNotification);

    const int hzMidiForNeedle = midiNoteFromHz(lastState.frequencyHz);
    const bool lockedNoteForNeedle = (pendingFrames >= kFramesToLockNote) && (hzMidiForNeedle == pendingMidi);
    needleDrawActive = lastState.signalPresent && !weakSignalHold && stableDisplayedMidi >= 0 && lockedNoteForNeedle;

    repaint();
}

void TunerOverlayComponent::paint(juce::Graphics& g)
{
    g.fillAll(bg());

    if (needleMeterArea.isEmpty())
        return;

    auto block = needleMeterArea.toFloat().reduced(6.0f, 4.0f);
    auto levelZone = block.removeFromBottom(26.0f);
    auto needleBand = block;

    g.setColour(juce::Colour(0xff1e252e));
    g.fillRoundedRectangle(needleBand, 8.0f);
    g.setColour(juce::Colour(0xff2a3340));
    g.drawRoundedRectangle(needleBand, 8.0f, 2.0f);

    const float cx = needleBand.getCentreX();
    const float wFull = needleMeterArea.toFloat().getWidth();
    const float inTuneHalf = wFull * 0.06f;

    g.setColour(good().withAlpha(0.28f));
    g.fillRoundedRectangle(juce::Rectangle<float>(cx - inTuneHalf, needleBand.getY(), inTuneHalf * 2.0f, needleBand.getHeight()),
                           6.0f);

    g.setColour(accent().withAlpha(0.45f));
    g.fillRect(juce::Rectangle<float>(cx - 1.2f, needleBand.getY(), 2.4f, needleBand.getHeight()));

    auto labelBand = needleBand;
    const float w = needleBand.getWidth();

    g.setColour(muted());
    g.setFont(juce::Font(14.0f));
    g.drawText("FLAT", labelBand.removeFromLeft(w * 0.14f), juce::Justification::centredLeft, false);
    g.drawText("SHARP", labelBand.removeFromRight(w * 0.14f), juce::Justification::centredRight, false);

    float centsDraw = needleDrawActive ? displayedCents : 0.0f;
    centsDraw = juce::jlimit(-50.0f, 50.0f, centsDraw);
    const float t = (centsDraw + 50.0f) / 100.0f;
    const float nx = labelBand.getX() + t * labelBand.getWidth();

    juce::Colour needleCol = juce::Colours::white;

    if (needleDrawActive)
    {
        const float ac = std::abs(displayedCents);

        if (ac <= 3.0f)
            needleCol = good().brighter(0.05f);
        else if (ac <= 10.0f)
            needleCol = warn();
        else
            needleCol = bad();
    }
    else
        needleCol = juce::Colours::white.withAlpha(0.35f);

    g.setColour(needleCol);
    g.fillRoundedRectangle(
        juce::Rectangle<float>(5.0f, labelBand.getHeight() * 0.88f).withCentre({ nx, labelBand.getCentreY() }),
        2.5f);

    /* Centered outward level meter */
    g.setColour(juce::Colour(0xff151a21));
    g.fillRoundedRectangle(levelZone, 5.0f);
    g.setColour(juce::Colour(0xff2a3340));
    g.drawRoundedRectangle(levelZone, 5.0f, 1.3f);

    const float lcx = levelZone.getCentreX();
    const float midY = levelZone.getCentreY();
    const float halfAvail = juce::jmax(4.0f, (levelZone.getWidth() * 0.5f) - 14.0f);
    const float halfH = levelZone.getHeight() * 0.38f;
    const float mono = lastState.signalPresent
                           ? juce::jlimit(0.0f, 1.0f, lastState.inputLevel)
                           : 0.0f;
    const float span = halfAvail * mono;

    g.setColour(accent().withAlpha(0.85f));
    if (span > 0.5f)
    {
        g.fillRoundedRectangle(juce::Rectangle<float>(lcx - span, midY - halfH, span, halfH * 2.0f).reduced(0.0f, 0.0f),
                               3.0f);
        g.fillRoundedRectangle(juce::Rectangle<float>(lcx, midY - halfH, span, halfH * 2.0f).reduced(0.0f, 0.0f),
                               3.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.fillRect(juce::Rectangle<float>(lcx - 1.0f, levelZone.getY() + 3.0f, 2.0f, levelZone.getHeight() - 6.0f));

    const bool clipLit = juce::Time::getApproximateMillisecondCounter() < clipHoldUntilMs;

    if (clipLit)
    {
        g.setColour(juce::Colours::red);
        const float dot = 7.0f;
        g.fillEllipse(juce::Rectangle<float>(dot, dot).withCentre({levelZone.getX() + 6.0f, midY}));
        g.fillEllipse(juce::Rectangle<float>(dot, dot).withCentre({levelZone.getRight() - 6.0f, midY}));
    }

    g.setColour(muted());
    g.setFont(juce::Font(11.0f));
    g.drawText("IN", levelZone.withTrimmedTop(levelZone.getHeight() * 0.62f), juce::Justification::centred, false);
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

    needleMeterArea = r.removeFromTop(100);
    r.removeFromTop(10);

    statusLabel.setBounds(r.removeFromTop(48));
    r.removeFromTop(6);
    footnoteLabel.setBounds(r.removeFromTop(44));
}

} // namespace forge7
