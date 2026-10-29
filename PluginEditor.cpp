#include "PluginEditor.h"

AuraEditor::AuraEditor (AuraProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    waveLabel.setText ("Waveform", juce::dontSendNotification);
    waveLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (waveLabel);

    wave.addItemList ({ "Saw", "Square", "Supersaw" }, 1);
    addAndMakeVisible (wave);
    waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "wave", wave);

    addKnob ("detune", "Detune");
    addKnob ("cutoff", "Cutoff");
    addKnob ("res",    "Resonance");
    addKnob ("gain",   "Gain");
    addKnob ("atk",    "Attack");
    addKnob ("dec",    "Decay");
    addKnob ("sus",    "Sustain");
    addKnob ("rel",    "Release");

    setResizable (true, true);
    setResizeLimits (480, 320, 1600, 900);
    setSize (600, 380);
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
    auto top = area.removeFromTop (36);
    wave.setBounds (top.removeFromRight (140));
    waveLabel.setBounds (top.removeFromRight (90));
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
