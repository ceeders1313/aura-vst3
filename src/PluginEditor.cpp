#include "PluginEditor.h"

AuraEditor::AuraEditor (AuraProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    waveLabel.setText ("Waveform", juce::dontSendNotification);
    waveLabel.setJustificationType (juce::Justification::centredRight);
    sourceLabel.setText ("Source", juce::dontSendNotification);
    sourceLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (waveLabel);
    addAndMakeVisible (sourceLabel);

    wave.addItemList ({ "Saw", "Square", "Supersaw" }, 1);
    source.addItemList ({ "Synth", "Sample", "Synth+Sample" }, 1);
    addAndMakeVisible (wave);
    addAndMakeVisible (source);
    waveAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "wave", wave);
    sourceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "source", source);

    addAndMakeVisible (oneShot);
    oneShotAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "oneshot", oneShot);

    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Select samples", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<AuraEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  juce::Array<juce::File> files;
                                  for (const auto& f : fc.getResults()) files.add (f);
                                  if (! files.isEmpty()) safe->loadAndSwitch (files);
                              });
    };

    statusLabel.setText (proc.getStatusText(), juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    addKnob ("detune", "Detune");
    addKnob ("cutoff", "Cutoff");
    addKnob ("res",    "Resonance");
    addKnob ("gain",   "Gain");
    addKnob ("atk",    "Attack");
    addKnob ("dec",    "Decay");
    addKnob ("sus",    "Sustain");
    addKnob ("rel",    "Release");

    setResizable (true, true);
    setResizeLimits (620, 400, 1600, 900);
    setSize (720, 470);
    startTimerHz (4);
}

void AuraEditor::addKnob (const char* paramId, const char* text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, k->slider);
    knobs.push_back (std::move (k));
}

void AuraEditor::loadAndSwitch (const juce::Array<juce::File>& files)
{
    proc.loadSamples (files);
    source.setSelectedId (2);        // bascule sur "Sample"
}

void AuraEditor::filesDropped (const juce::StringArray& files, int, int)
{
    juce::Array<juce::File> list;
    for (const auto& s : files)
    {
        juce::File f (s);
        if (f.isDirectory())
        {
            for (const auto& e : f.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac"))
                list.add (e);
        }
        else
        {
            list.add (f);
        }
    }
    if (! list.isEmpty()) loadAndSwitch (list);
}

void AuraEditor::timerCallback()
{
    statusLabel.setText (proc.getStatusText(), juce::dontSendNotification);
}

void AuraEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161c));
    g.setColour (juce::Colour (0xff4fc3f7));
    g.setFont (juce::FontOptions (22.f, juce::Font::bold));
    g.drawText ("AURA", 16, 10, 120, 30, juce::Justification::centredLeft);
}

void AuraEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto top = area.removeFromTop (30);
    top.removeFromLeft (110);                 // place pour le titre
    wave.setBounds (top.removeFromRight (130));
    waveLabel.setBounds (top.removeFromRight (70));
    top.removeFromRight (10);
    source.setBounds (top.removeFromRight (130));
    sourceLabel.setBounds (top.removeFromRight (60));

    area.removeFromTop (8);
    auto row2 = area.removeFromTop (30);
    loadButton.setBounds (row2.removeFromLeft (150));
    row2.removeFromLeft (10);
    oneShot.setBounds (row2.removeFromLeft (100));
    row2.removeFromLeft (10);
    statusLabel.setBounds (row2);

    area.removeFromTop (10);
    const int cols = 4, rows = 2;
    const int w = area.getWidth() / cols, h = area.getHeight() / rows;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (area.getX() + int (i % cols) * w,
                                          area.getY() + int (i / cols) * h, w, h).reduced (6);
        knobs[i]->label.setBounds (cell.removeFromTop (20));
        knobs[i]->slider.setBounds (cell);
    }
}
