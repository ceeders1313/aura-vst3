#include "PluginEditor.h"

static const juce::Colour kAccent (0xff4fc3f7);

// ---------------- KnobSet ----------------
void KnobSet::add (const char* id, const char* text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    parent.addAndMakeVisible (k->slider);
    parent.addAndMakeVisible (k->label);
    k->attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, k->slider);
    knobs.push_back (std::move (k));
}

void KnobSet::layout (juce::Rectangle<int> area, int cols)
{
    if (knobs.empty() || cols <= 0) return;
    const int count = (int) knobs.size();
    const int rowsN = (count + cols - 1) / cols;
    const int w = area.getWidth() / cols, h = area.getHeight() / rowsN;
    for (int i = 0; i < count; ++i)
    {
        auto cell = juce::Rectangle<int> (area.getX() + (i % cols) * w,
                                          area.getY() + (i / cols) * h, w, h).reduced (4);
        knobs[(size_t) i]->label.setBounds (cell.removeFromTop (18));
        knobs[(size_t) i]->slider.setBounds (cell);
    }
}

// ---------------- MsegEditor ----------------
juce::Point<float> MsegEditor::toPx (float x, float y) const
{
    const auto a = area();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

void MsegEditor::fromPx (juce::Point<float> p, float& x, float& y) const
{
    const auto a = area();
    x = (p.x - a.getX()) / a.getWidth();
    y = (a.getBottom() - p.y) / a.getHeight();
}

int MsegEditor::hitPoint (juce::Point<float> p) const
{
    for (int i = 0; i < shape.n; ++i)
        if (toPx (shape.x[i], shape.y[i]).getDistanceFrom (p) < 10.f) return i;
    return -1;
}

int MsegEditor::hitCurve (juce::Point<float> p) const
{
    for (int i = 0; i < shape.n - 1; ++i)
    {
        const float mx = 0.5f * (shape.x[i] + shape.x[i + 1]);
        if (toPx (mx, shape.eval (mx)).getDistanceFrom (p) < 9.f) return i;
    }
    return -1;
}

void MsegEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e1015));
    const auto a = area();

    g.setColour (juce::Colour (0xff222630));
    for (int i = 0; i <= 8; ++i)
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * float (i) / 8.f), a.getY(), a.getBottom());
    for (int i = 0; i <= 4; ++i)
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * float (i) / 4.f), a.getX(), a.getRight());

    juce::Path path;
    const int steps = 300;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = float (i) / float (steps);
        const auto pt = toPx (t, shape.eval (t));
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    juce::Path fill (path);
    fill.lineTo (a.getRight(), a.getBottom());
    fill.lineTo (a.getX(), a.getBottom());
    fill.closeSubPath();
    g.setColour (kAccent.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (kAccent);
    g.strokePath (path, juce::PathStrokeType (2.f));

    g.setColour (juce::Colours::orange);
    for (int i = 0; i < shape.n - 1; ++i)
    {
        const float mx = 0.5f * (shape.x[i] + shape.x[i + 1]);
        const auto c = toPx (mx, shape.eval (mx));
        juce::Path d;
        d.addQuadrilateral (c.x, c.y - 5.f, c.x + 5.f, c.y, c.x, c.y + 5.f, c.x - 5.f, c.y);
        g.fillPath (d);
    }
    for (int i = 0; i < shape.n; ++i)
    {
        const auto c = toPx (shape.x[i], shape.y[i]);
        g.setColour (juce::Colours::white);
        g.fillEllipse (c.x - 5.f, c.y - 5.f, 10.f, 10.f);
        g.setColour (kAccent);
        g.drawEllipse (c.x - 5.f, c.y - 5.f, 10.f, 10.f, 2.f);
    }
}

void MsegEditor::mouseDown (const juce::MouseEvent& e)
{
    dragPoint = hitPoint (e.position);
    dragCurve = -1;
    if (dragPoint < 0)
    {
        dragCurve = hitCurve (e.position);
        if (dragCurve >= 0)
        {
            curveStart = shape.curve[dragCurve];
            dragStartY = e.position.y;
        }
    }
}

void MsegEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragPoint >= 0)
    {
        float x, y;
        fromPx (e.position, x, y);
        y = juce::jlimit (0.f, 1.f, y);
        if (dragPoint == 0) x = 0.f;
        else if (dragPoint == shape.n - 1) x = 1.f;
        else x = juce::jlimit (shape.x[dragPoint - 1] + 0.002f, shape.x[dragPoint + 1] - 0.002f, x);
        shape.x[dragPoint] = x;
        shape.y[dragPoint] = y;
        notify();
    }
    else if (dragCurve >= 0)
    {
        const float dy = (dragStartY - e.position.y) / (area().getHeight() * 0.5f);
        const bool rising = shape.y[dragCurve + 1] >= shape.y[dragCurve];
        shape.curve[dragCurve] = juce::jlimit (-1.f, 1.f, rising ? curveStart - dy : curveStart + dy);
        notify();
    }
}

void MsegEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hp = hitPoint (e.position);
    if (hp > 0 && hp < shape.n - 1)               // supprime un point interieur
    {
        for (int i = hp; i < shape.n - 1; ++i)
        {
            shape.x[i] = shape.x[i + 1];
            shape.y[i] = shape.y[i + 1];
            shape.curve[i] = shape.curve[i + 1];
        }
        --shape.n;
        shape.curve[hp - 1] = 0.f;
        notify();
        return;
    }
    if (hp >= 0 || shape.n >= aura::MsegShape::kMax) return;

    float x, y;
    fromPx (e.position, x, y);
    x = juce::jlimit (0.002f, 0.998f, x);
    y = juce::jlimit (0.f, 1.f, y);

    int ins = 1;
    while (ins < shape.n && shape.x[ins] < x) ++ins;    // x[ins-1] < x <= x[ins]
    for (int i = shape.n; i > ins; --i)
    {
        shape.x[i] = shape.x[i - 1];
        shape.y[i] = shape.y[i - 1];
        shape.curve[i] = shape.curve[i - 1];
    }
    shape.x[ins] = x;
    shape.y[ins] = y;
    shape.curve[ins] = 0.f;
    shape.curve[ins - 1] = 0.f;
    ++shape.n;
    notify();
}

// ---------------- SoundPage ----------------
SoundPage::SoundPage (AuraProcessor& p) : proc (p), knobs (*this, p.apvts)
{
    source.init (*this, p.apvts, "source", "Source", { "Synth", "Sample", "Synth+Sample" });
    wave.init   (*this, p.apvts, "wave", "Waveform", { "Saw", "Square", "Wavetable", "FM" });
    fmAlgo.init (*this, p.apvts, "fmalgo", "FM Algo", { "2-Op", "3-Op Stack", "3-Op Parallel" });

    addAndMakeVisible (oneShot);
    oneShotAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "oneshot", oneShot);

    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Select samples", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<SoundPage> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  juce::Array<juce::File> files;
                                  for (const auto& f : fc.getResults()) files.add (f);
                                  if (! files.isEmpty()) safe->loadFiles (files);
                              });
    };

    statusLabel.setText (proc.getStatusText(), juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    knobs.add ("unison", "Unison");
    knobs.add ("detune", "Detune");
    knobs.add ("wtpos",  "WT Position");
    knobs.add ("gain",   "Gain");
    knobs.add ("cutoff", "Cutoff");
    knobs.add ("res",    "Resonance");
    knobs.add ("fmr1",   "FM Ratio 1");
    knobs.add ("fmr2",   "FM Ratio 2");
    knobs.add ("fmidx",  "FM Index");
    knobs.add ("fmfb",   "FM Feedback");
    knobs.add ("atk",    "Attack");
    knobs.add ("dec",    "Decay");
    knobs.add ("sus",    "Sustain");
    knobs.add ("rel",    "Release");

    startTimerHz (4);
}

void SoundPage::loadFiles (const juce::Array<juce::File>& files)
{
    proc.loadSamples (files);
    source.box.setSelectedId (2);
}

void SoundPage::timerCallback()
{
    statusLabel.setText (proc.getStatusText(), juce::dontSendNotification);
}

void SoundPage::resized()
{
    auto a = getLocalBounds().reduced (8);
    auto r1 = a.removeFromTop (28);
    source.place (r1, 55, 120);
    wave.place (r1, 65, 110);
    fmAlgo.place (r1, 60, 130);

    a.removeFromTop (6);
    auto r2 = a.removeFromTop (28);
    loadButton.setBounds (r2.removeFromLeft (140));
    r2.removeFromLeft (10);
    oneShot.setBounds (r2.removeFromLeft (100));
    r2.removeFromLeft (10);
    statusLabel.setBounds (r2);

    a.removeFromTop (6);
    knobs.layout (a, 5);
}

// ---------------- ModPage ----------------
ModPage::ModPage (AuraProcessor& p) : knobs (*this, p.apvts)
{
    const auto srcItems = aura::namesToArray (aura::kSrcNames, aura::kNumSrc);
    const auto dstItems = aura::namesToArray (aura::kDstNames, aura::kNumDst);

    for (int i = 0; i < aura::kSlots; ++i)
    {
        auto r = std::make_unique<Row>();
        r->num.setText (juce::String (i + 1), juce::dontSendNotification);
        r->num.setJustificationType (juce::Justification::centred);
        r->src.addItemList (srcItems, 1);
        r->dst.addItemList (dstItems, 1);
        r->amt.setSliderStyle (juce::Slider::LinearHorizontal);
        r->amt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        addAndMakeVisible (r->num);
        addAndMakeVisible (r->src);
        addAndMakeVisible (r->dst);
        addAndMakeVisible (r->amt);
        r->aS = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, aura::kSlotSrcId[i], r->src);
        r->aD = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, aura::kSlotDstId[i], r->dst);
        r->aA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, aura::kSlotAmtId[i], r->amt);
        rows.push_back (std::move (r));
    }

    const juce::StringArray shapes { "Sine", "Triangle", "Saw", "Square", "S&H" };
    lfo1Shape.init (*this, p.apvts, "lfo1shape", "LFO 1", shapes);
    lfo2Shape.init (*this, p.apvts, "lfo2shape", "LFO 2", shapes);

    knobs.add ("lfo1rate", "LFO1 Rate");
    knobs.add ("lfo2rate", "LFO2 Rate");
    knobs.add ("matk", "Env Attack");
    knobs.add ("mdec", "Env Decay");
    knobs.add ("msus", "Env Sustain");
    knobs.add ("mrel", "Env Release");
    knobs.add ("macro1", "Macro 1");
    knobs.add ("macro2", "Macro 2");
    knobs.add ("macro3", "Macro 3");
    knobs.add ("macro4", "Macro 4");
}

void ModPage::resized()
{
    auto a = getLocalBounds().reduced (8);
    for (auto& r : rows)
    {
        auto row = a.removeFromTop (26);
        r->num.setBounds (row.removeFromLeft (24));
        r->src.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (6);
        r->dst.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (6);
        r->amt.setBounds (row);
        a.removeFromTop (2);
    }
    a.removeFromTop (6);
    auto r = a.removeFromTop (28);
    lfo1Shape.place (r, 50, 110);
    lfo2Shape.place (r, 50, 110);
    a.removeFromTop (4);
    knobs.layout (a, 5);
}

// ---------------- MsegPage ----------------
MsegPage::MsegPage (AuraProcessor& p) : proc (p), knobs (*this, p.apvts)
{
    addAndMakeVisible (editor);
    editor.setShape (proc.getMseg());
    editor.onChange = [this] (const aura::MsegShape& s) { proc.setMseg (s); };

    hint.setText ("Double-click: add / delete point   |   Drag orange diamond: curve   |   "
                  "Use MSEG as a source in the Mod tab", juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (hint);

    addAndMakeVisible (loop);
    loopAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "msegloop", loop);
    knobs.add ("msegtime", "Time (s)");
}

void MsegPage::resized()
{
    auto a = getLocalBounds().reduced (8);
    auto bottom = a.removeFromBottom (110);
    hint.setBounds (a.removeFromBottom (22));
    editor.setBounds (a);

    loop.setBounds (bottom.removeFromLeft (80).withSizeKeepingCentre (70, 28));
    knobs.layout (bottom.removeFromLeft (140), 1);
}

// ---------------- AuraEditor ----------------
AuraEditor::AuraEditor (AuraProcessor& p)
    : AudioProcessorEditor (&p), proc (p), sound (p), mod (p), mseg (p)
{
    tabs.addTab ("Sound", juce::Colour (0xff1b1e26), &sound, false);
    tabs.addTab ("Mod",   juce::Colour (0xff1b1e26), &mod, false);
    tabs.addTab ("MSEG",  juce::Colour (0xff1b1e26), &mseg, false);
    addAndMakeVisible (tabs);

    setResizable (true, true);
    setResizeLimits (720, 520, 1800, 1000);
    setSize (800, 620);
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
    if (! list.isEmpty()) sound.loadFiles (list);
}

void AuraEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161c));
    g.setColour (kAccent);
    g.setFont (juce::FontOptions (22.f, juce::Font::bold));
    g.drawText ("AURA", 14, 6, 120, 28, juce::Justification::centredLeft);
}

void AuraEditor::resized()
{
    auto a = getLocalBounds();
    a.removeFromTop (38);
    tabs.setBounds (a);
}
