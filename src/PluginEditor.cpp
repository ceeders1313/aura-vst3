#include "PluginEditor.h"

using namespace aura;

static const juce::Colour kAccent (0xff4fc3f7);
static const juce::Colour kPanel (0xff1b1e26);

// ---------------- KnobSet ----------------
void KnobSet::add (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    parent.addAndMakeVisible (k->slider);
    parent.addAndMakeVisible (k->label);
    k->attach = std::make_unique<APVTS_::SliderAttachment> (apvts, id, k->slider);
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
    if (hp > 0 && hp < shape.n - 1)
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
    if (hp >= 0 || shape.n >= MsegShape::kMax) return;

    float x, y;
    fromPx (e.position, x, y);
    x = juce::jlimit (0.002f, 0.998f, x);
    y = juce::jlimit (0.f, 1.f, y);

    int ins = 1;
    while (ins < shape.n && shape.x[ins] < x) ++ins;
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
SoundPage::SoundPage (AuraProcessor& p, int layerIndex)
    : proc (p), layer (layerIndex), knobs (*this, p.apvts)
{
    auto& s = p.apvts;
    source.init (*this, s, lpId (layer, LP_Source), "Source", { "Synth", "Sample", "Synth+Sample" });
    wave.init   (*this, s, lpId (layer, LP_Wave), "Waveform", { "Saw", "Square", "Wavetable", "FM" });
    fmAlgo.init (*this, s, lpId (layer, LP_FmAlgo), "FM Algo", { "2-Op", "3-Op Stack", "3-Op Parallel" });
    oneShot.init (*this, s, lpId (layer, LP_OneShot), "One-shot");

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

    statusLabel.setText (proc.getStatusText (layer), juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    knobs.add (lpId (layer, LP_Unison), "Unison");
    knobs.add (lpId (layer, LP_Detune), "Detune");
    knobs.add (lpId (layer, LP_WtPos),  "WT Position");
    knobs.add (lpId (layer, LP_Level),  "Level");
    knobs.add (lpId (layer, LP_Cutoff), "Cutoff");
    knobs.add (lpId (layer, LP_Res),    "Resonance");
    knobs.add (lpId (layer, LP_FmR1),   "FM Ratio 1");
    knobs.add (lpId (layer, LP_FmR2),   "FM Ratio 2");
    knobs.add (lpId (layer, LP_FmIdx),  "FM Index");
    knobs.add (lpId (layer, LP_FmFb),   "FM Feedback");
    knobs.add (lpId (layer, LP_Atk),    "Attack");
    knobs.add (lpId (layer, LP_Dec),    "Decay");
    knobs.add (lpId (layer, LP_Sus),    "Sustain");
    knobs.add (lpId (layer, LP_Rel),    "Release");

    startTimerHz (4);
}

void SoundPage::loadFiles (const juce::Array<juce::File>& files)
{
    proc.loadSamples (layer, files);
    source.box.setSelectedId (2);
}

void SoundPage::timerCallback()
{
    statusLabel.setText (proc.getStatusText (layer), juce::dontSendNotification);
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
    oneShot.button.setBounds (r2.removeFromLeft (100));
    r2.removeFromLeft (10);
    statusLabel.setBounds (r2);

    a.removeFromTop (6);
    knobs.layout (a, 5);
}

// ---------------- ModPage ----------------
ModPage::ModPage (AuraProcessor& p, int layer) : knobs (*this, p.apvts)
{
    const auto srcItems = namesToArray (kSrcNames, kNumSrc);
    const auto dstItems = namesToArray (kDstNames, kNumDst);

    for (int i = 0; i < kSlots; ++i)
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
        r->aS = std::make_unique<APVTS_::ComboBoxAttachment> (p.apvts, lpId (layer, lpSlot (i, 0)), r->src);
        r->aD = std::make_unique<APVTS_::ComboBoxAttachment> (p.apvts, lpId (layer, lpSlot (i, 1)), r->dst);
        r->aA = std::make_unique<APVTS_::SliderAttachment> (p.apvts, lpId (layer, lpSlot (i, 2)), r->amt);
        rows.push_back (std::move (r));
    }

    const juce::StringArray shapes { "Sine", "Triangle", "Saw", "Square", "S&H" };
    lfo1Shape.init (*this, p.apvts, lpId (layer, LP_Lfo1Shape), "LFO 1", shapes);
    lfo2Shape.init (*this, p.apvts, lpId (layer, LP_Lfo2Shape), "LFO 2", shapes);

    knobs.add (lpId (layer, LP_Lfo1Rate), "LFO1 Rate");
    knobs.add (lpId (layer, LP_Lfo2Rate), "LFO2 Rate");
    knobs.add (lpId (layer, LP_MAtk), "Env Attack");
    knobs.add (lpId (layer, LP_MDec), "Env Decay");
    knobs.add (lpId (layer, LP_MSus), "Env Sustain");
    knobs.add (lpId (layer, LP_MRel), "Env Release");
    knobs.add (lpId (layer, LP_Macro1), "Macro 1");
    knobs.add (lpId (layer, LP_Macro2), "Macro 2");
    knobs.add (lpId (layer, LP_Macro3), "Macro 3");
    knobs.add (lpId (layer, LP_Macro4), "Macro 4");
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

// ---------------- MsegPage (2 MSEG) ----------------
MsegPage::MsegPage (AuraProcessor& p, int layerIndex)
    : proc (p), layer (layerIndex), knobsA (*this, p.apvts), knobsB (*this, p.apvts)
{
    for (int k = 0; k < 2; ++k)
    {
        addAndMakeVisible (editor[k]);
        editor[k].setShape (proc.getMseg (layer, k));
        editor[k].onChange = [this, k] (const MsegShape& s) { proc.setMseg (layer, k, s); };
    }
    loop[0].init (*this, p.apvts, lpId (layer, LP_Mseg1Loop), "Loop");
    loop[1].init (*this, p.apvts, lpId (layer, LP_Mseg2Loop), "Loop");
    knobsA.add (lpId (layer, LP_Mseg1Time), "MSEG 1 Time");
    knobsB.add (lpId (layer, LP_Mseg2Time), "MSEG 2 Time");

    hint.setText ("Double-click: add / delete point  |  Drag orange diamond: curve  |  "
                  "Assign MSEG 1 / MSEG 2 as sources in the Mod tab", juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (hint);
}

void MsegPage::resized()
{
    auto a = getLocalBounds().reduced (8);
    hint.setBounds (a.removeFromBottom (22));
    const int h = a.getHeight() / 2;
    for (int k = 0; k < 2; ++k)
    {
        auto half = (k == 0) ? a.removeFromTop (h) : a;
        auto side = half.removeFromRight (130);
        editor[k].setBounds (half.reduced (0, 2));
        loop[k].button.setBounds (side.removeFromBottom (28));
        (k == 0 ? knobsA : knobsB).layout (side, 1);
    }
}

// ---------------- LayerPage ----------------
LayerPage::LayerPage (AuraProcessor& p, int layer)
    : sound (p, layer), mod (p, layer), mseg (p, layer)
{
    enable.init (*this, p.apvts, lpId (layer, LP_Enable), "Layer " + juce::String (layer + 1) + " enabled");
    inner.addTab ("Sound", kPanel, &sound, false);
    inner.addTab ("Mod",   kPanel, &mod, false);
    inner.addTab ("MSEG",  kPanel, &mseg, false);
    addAndMakeVisible (inner);
}

void LayerPage::resized()
{
    auto a = getLocalBounds().reduced (4);
    enable.button.setBounds (a.removeFromTop (26).removeFromLeft (200));
    inner.setBounds (a);
}

// ---------------- RoutingPage ----------------
RoutingPage::RoutingPage (AuraProcessor& p)
{
    struct Col { int lp; const char* text; };
    static const Col cols[] = {
        { LP_KeyLo, "Key Lo" }, { LP_KeyHi, "Key Hi" }, { LP_VelLo, "Vel Lo" }, { LP_VelHi, "Vel Hi" },
        { LP_Transpose, "Transpose" }, { LP_Level, "Level" }, { LP_Pan, "Pan" },
        { LP_SendCho, "Chorus" }, { LP_SendDly, "Delay" }, { LP_SendRev, "Reverb" } };

    auto addHeader = [this] (const char* t)
    {
        auto l = std::make_unique<juce::Label>();
        l->setText (t, juce::dontSendNotification);
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, kAccent);
        addAndMakeVisible (*l);
        headers.push_back (std::move (l));
    };
    addHeader ("Layer");
    addHeader ("On");
    for (const auto& c : cols) addHeader (c.text);
    addHeader ("Arp");

    for (int l = 0; l < kLayers; ++l)
    {
        auto r = std::make_unique<Row>();
        r->name.setText ("Layer " + juce::String (l + 1), juce::dontSendNotification);
        addAndMakeVisible (r->name);
        r->en.init (*this, p.apvts, lpId (l, LP_Enable), "");
        r->arp.init (*this, p.apvts, lpId (l, LP_Arp), "");
        for (const auto& c : cols)
        {
            auto k = std::make_unique<MiniKnob>();
            k->s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            k->s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
            addAndMakeVisible (k->s);
            k->a = std::make_unique<APVTS_::SliderAttachment> (p.apvts, lpId (l, c.lp), k->s);
            r->knobs.push_back (std::move (k));
        }
        rows.push_back (std::move (r));
    }
}

void RoutingPage::resized()
{
    auto a = getLocalBounds().reduced (8);
    const int nameW = 64, onW = 40, arpW = 40;
    const int nk = rows.empty() ? 10 : (int) rows[0]->knobs.size();
    const int kw = (a.getWidth() - nameW - onW - arpW) / std::max (1, nk);

    auto hdr = a.removeFromTop (22);
    {
        size_t hi = 0;
        headers[hi++]->setBounds (hdr.removeFromLeft (nameW));
        headers[hi++]->setBounds (hdr.removeFromLeft (onW));
        for (int k = 0; k < nk; ++k) headers[hi++]->setBounds (hdr.removeFromLeft (kw));
        headers[hi++]->setBounds (hdr.removeFromLeft (arpW));
    }

    const int rowH = std::min (100, a.getHeight() / std::max (1, (int) rows.size()));
    for (auto& r : rows)
    {
        auto row = a.removeFromTop (rowH);
        r->name.setBounds (row.removeFromLeft (nameW));
        r->en.button.setBounds (row.removeFromLeft (onW).withSizeKeepingCentre (24, 24));
        for (auto& k : r->knobs) k->s.setBounds (row.removeFromLeft (kw).reduced (2));
        r->arp.button.setBounds (row.removeFromLeft (arpW).withSizeKeepingCentre (24, 24));
    }
}

// ---------------- FxPage ----------------
FxPage::FxPage (AuraProcessor& p)
    : sat (*this, p.apvts, "Saturation"), cho (*this, p.apvts, "Chorus (send)"),
      dly (*this, p.apvts, "Delay (send)"), rev (*this, p.apvts, "Reverb FDN (send)"),
      lim (*this, p.apvts, "Limiter"), mst (*this, p.apvts, "Master")
{
    auto& s = p.apvts;
    satOn.init (*this, s, "satOn", "On");
    satMode.init (*this, s, "satMode", "Mode", { "Tanh", "Hard Clip", "Asymmetric" });
    sat.knobs.add ("satDrive", "Drive (dB)");
    sat.knobs.add ("satMix", "Mix");

    cho.knobs.add ("choRate", "Rate");
    cho.knobs.add ("choDepth", "Depth");
    cho.knobs.add ("choRet", "Return");

    dlyPing.init (*this, s, "dlyPing", "Ping-pong");
    dly.knobs.add ("dlyTime", "Time (ms)");
    dly.knobs.add ("dlyFb", "Feedback");
    dly.knobs.add ("dlyDamp", "Damping");
    dly.knobs.add ("dlyRet", "Return");

    rev.knobs.add ("revDecay", "Decay (s)");
    rev.knobs.add ("revSize", "Size");
    rev.knobs.add ("revDamp", "Damping");
    rev.knobs.add ("revPre", "Pre-delay");
    rev.knobs.add ("revRet", "Return");

    limOn.init (*this, s, "limOn", "On");
    lim.knobs.add ("limCeil", "Ceiling (dB)");
    lim.knobs.add ("limRel", "Release (ms)");

    mst.knobs.add ("master", "Master");
}

void FxPage::resized()
{
    auto a = getLocalBounds().reduced (8);
    const int colW = a.getWidth() / 3, rowH = a.getHeight() / 2;
    auto cell = [&] (int c, int r)
    {
        return juce::Rectangle<int> (a.getX() + c * colW, a.getY() + r * rowH, colW, rowH).reduced (4);
    };

    {   auto r = cell (0, 0);
        sat.title.setBounds (r.removeFromTop (24));
        auto band = r.removeFromTop (28);
        satOn.button.setBounds (band.removeFromLeft (60));
        satMode.place (band, 45, 120);
        sat.knobs.layout (r, 2); }
    {   auto r = cell (1, 0);
        cho.title.setBounds (r.removeFromTop (24));
        r.removeFromTop (28);
        cho.knobs.layout (r, 3); }
    {   auto r = cell (2, 0);
        dly.title.setBounds (r.removeFromTop (24));
        dlyPing.button.setBounds (r.removeFromTop (28).removeFromLeft (120));
        dly.knobs.layout (r, 4); }
    {   auto r = cell (0, 1);
        rev.title.setBounds (r.removeFromTop (24));
        r.removeFromTop (28);
        rev.knobs.layout (r, 5); }
    {   auto r = cell (1, 1);
        lim.title.setBounds (r.removeFromTop (24));
        limOn.button.setBounds (r.removeFromTop (28).removeFromLeft (80));
        lim.knobs.layout (r, 2); }
    {   auto r = cell (2, 1);
        mst.title.setBounds (r.removeFromTop (24));
        r.removeFromTop (28);
        mst.knobs.layout (r, 1); }
}

// ---------------- ArpPage ----------------
ArpPage::ArpPage (AuraProcessor& p) : knobs (*this, p.apvts)
{
    on.init (*this, p.apvts, "arpon", "Arpeggiator on");
    latch.init (*this, p.apvts, "arplatch", "Latch");
    mode.init (*this, p.apvts, "arpmode", "Mode", { "Up", "Down", "Up/Down", "Random", "As Played", "Chord" });
    rate.init (*this, p.apvts, "arprate", "Rate", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" });
    knobs.add ("arpoct", "Octaves");
    knobs.add ("arpgate", "Gate");
    knobs.add ("arpswing", "Swing");

    hint.setText ("The arpeggiator follows the host tempo and transport. "
                  "Enable 'Arp' per layer in the Routing tab; other layers keep playing the raw keys.",
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, juce::Colours::grey);
    hint.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (hint);
}

void ArpPage::resized()
{
    auto a = getLocalBounds().reduced (12);
    auto r1 = a.removeFromTop (30);
    on.button.setBounds (r1.removeFromLeft (170));
    latch.button.setBounds (r1.removeFromLeft (100));
    a.removeFromTop (6);
    auto r2 = a.removeFromTop (30);
    mode.place (r2, 50, 130);
    rate.place (r2, 50, 100);
    a.removeFromTop (10);
    knobs.layout (a.removeFromTop (130), 3);
    hint.setBounds (a);
}

// ---------------- AuraEditor ----------------
AuraEditor::AuraEditor (AuraProcessor& p)
    : AudioProcessorEditor (&p), proc (p), routing (p), fx (p), arpPage (p)
{
    for (int l = 0; l < kLayers; ++l)
    {
        layerPages.push_back (std::make_unique<LayerPage> (p, l));
        tabs.addTab ("Layer " + juce::String (l + 1), kPanel, layerPages.back().get(), false);
    }
    tabs.addTab ("Routing", kPanel, &routing, false);
    tabs.addTab ("FX",      kPanel, &fx, false);
    tabs.addTab ("Arp",     kPanel, &arpPage, false);
    addAndMakeVisible (tabs);

    setResizable (true, true);
    setResizeLimits (820, 600, 1800, 1100);
    setSize (940, 700);
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
    if (list.isEmpty()) return;

    int idx = tabs.getCurrentTabIndex();
    if (idx < 0 || idx >= kLayers) idx = 0;
    layerPages[(size_t) idx]->loadFiles (list);
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
