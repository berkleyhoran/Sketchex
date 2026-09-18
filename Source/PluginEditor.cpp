#include "PluginEditor.h"

using L = sketchex::SketchexLookAndFeel;

SketchexAudioProcessorEditor::SketchexAudioProcessorEditor(SketchexAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), canvas(p), controls(p, canvas)
{
    addAndMakeVisible(canvas);
    addAndMakeVisible(controls);

    playButton.setClickingTogglesState(false);
    playButton.getProperties().set(L::accentProperty, (juce::int64) L::accent2().getARGB());
    playButton.onClick = [this]
    {
        processor.setInternalPlaying(! processor.isInternalPlaying());
        updateTransportButton();
    };
    playButton.setTooltip("Internal clock. Inside a DAW, the host transport takes over automatically when it plays.");
    addAndMakeVisible(playButton);

    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setRange(40.0, 240.0, 1.0);
    bpmSlider.setValue(processor.getInternalBpm(), juce::dontSendNotification);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
    bpmSlider.getProperties().set(L::accentProperty, (juce::int64) L::accent2().getARGB());
    bpmSlider.onValueChange = [this] { processor.setInternalBpm(bpmSlider.getValue()); };
    addAndMakeVisible(bpmSlider);
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setFont(L::titleFont(12.0f));
    bpmLabel.setColour(juce::Label::textColourId, L::inkSoft());
    addAndMakeVisible(bpmLabel);

    checkUpdatesButton.onClick = [this] { checkForUpdates(); };
    addAndMakeVisible(checkUpdatesButton);

    canvas.onSketchChanged = [this] { controls.refresh(); };

    // Installed *after* every child exists so the change propagates to
    // all of them (Slider text boxes are created with whatever LnF is
    // current at construction and only rebuilt on lookAndFeelChanged).
    setLookAndFeel(&lookAndFeel);
    sendLookAndFeelChange();

    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(860, 600, 2400, 1600);
    setSize(1000, 700);
    updateTransportButton();
    startTimerHz(10);
}

SketchexAudioProcessorEditor::~SketchexAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SketchexAudioProcessorEditor::timerCallback()
{
    updateTransportButton();
    if (! bpmSlider.isMouseButtonDown())
    {
        const double bpm = processor.getCurrentBpm();
        if (std::abs(bpm - bpmSlider.getValue()) > 0.5)
            bpmSlider.setValue(bpm, juce::dontSendNotification);
    }
}

void SketchexAudioProcessorEditor::updateTransportButton()
{
    const bool host = processor.isHostDrivingTransport();
    const bool internal = processor.isInternalPlaying();
    playButton.setButtonText(host ? "Host" : internal ? "Stop" : "Play");
    playButton.setToggleState(host || internal, juce::dontSendNotification);
    playButton.setEnabled(! host);
    bpmSlider.setEnabled(! host);
}

bool SketchexAudioProcessorEditor::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)
        || k == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        canvas.undo();
        controls.refresh();
        return true;
    }
    if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || k == juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0))
    {
        canvas.redo();
        controls.refresh();
        return true;
    }
    if (k == juce::KeyPress::spaceKey && ! processor.isHostDrivingTransport())
    {
        playButton.triggerClick();
        return true;
    }
    if (k.getTextCharacter() == 'e') { canvas.setTool(sketchex::SketchCanvas::Tool::erase); controls.refresh(); return true; }
    if (k.getTextCharacter() == 'd') { canvas.setTool(sketchex::SketchCanvas::Tool::draw); controls.refresh(); return true; }
    return false;
}

void SketchexAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // Sky gradient
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFDDF1FF), 0.0f, 0.0f,
                                           juce::Colour(0xFFF6E9FF), 0.0f, r.getHeight(), false));
    g.fillAll();
    // Soft diagonal sheen
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.55f), 0.0f, 0.0f,
                                           juce::Colours::white.withAlpha(0.0f), r.getWidth() * 0.6f, r.getHeight(), false));
    g.fillAll();

    // Title
    auto header = getLocalBounds().removeFromTop(48).reduced(16, 0);
    g.setFont(L::titleFont(26.0f));
    g.setColour(L::ink());
    g.drawText("Sketchex", header, juce::Justification::centredLeft, false);
    g.setFont(L::uiFont(12.0f));
    g.setColour(L::inkSoft());
    g.drawText(juce::String(juce::CharPointer_UTF8("draw-to-MIDI sequencer  \xc2\xb7  gexex  \xc2\xb7  v")) + juce::String(ProjectInfo::versionString),
               header.withTrimmedLeft(118).withTrimmedTop(6), juce::Justification::centredLeft, false);
}

void SketchexAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    auto header = b.removeFromTop(48).reduced(16, 8);
    checkUpdatesButton.setBounds(header.removeFromRight(140));
    header.removeFromRight(12);
    playButton.setBounds(header.removeFromRight(72));
    header.removeFromRight(10);
    bpmSlider.setBounds(header.removeFromRight(190));
    bpmLabel.setBounds(header.removeFromRight(36));

    b.reduce(12, 4);
    controls.setBounds(b.removeFromBottom(226));
    b.removeFromBottom(8);
    canvas.setBounds(b);
}

void SketchexAudioProcessorEditor::checkForUpdates()
{
    checkUpdatesButton.setEnabled(false);
    checkUpdatesButton.setButtonText("Checking...");

    updateChecker.checkAsync(
        [this](UpdateChecker::Result result)
        {
            checkUpdatesButton.setEnabled(true);
            checkUpdatesButton.setButtonText("Check for Updates");

            if (! result.checkedOk)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Update Check Failed",
                                                       result.errorMessage);
                return;
            }
            if (! result.updateAvailable)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Up to Date",
                                                       "You're running the latest version.");
                return;
            }
            const auto releaseUrl = result.releaseUrl;
            juce::NativeMessageBox::showOkCancelBox(
                juce::MessageBoxIconType::InfoIcon, "Update Available",
                "Version " + result.latestVersion + " is available (you're on v"
                    + juce::String(ProjectInfo::versionString) + "). Open the release page to download it?",
                this,
                juce::ModalCallbackFunction::create(
                    [releaseUrl](int okPressed)
                    {
                        if (okPressed != 0 && releaseUrl.isNotEmpty())
                            juce::URL(releaseUrl).launchInDefaultBrowser();
                    }));
        });
}
