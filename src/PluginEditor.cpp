#include "PluginEditor.h"

using namespace aura;

static const juce::StringArray kRateItems { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

// ============================================================================
// Theme
// ============================================================================
juce::String ui::layerTypeText (APVTS_& s, int layer)
{
    const int src  = juce::roundToInt (s.getRawParameterValue (lpId (layer, LP_Source))->load());
    const int wave = juce::roundToInt (s.getRawParameterValue (lpId (layer, LP_Wave))->load());
    static const char* const waves[] = { "SAW", "SQR", "WT", "FM", "TRI", "SINE" };
    const juce::String w (waves[juce::jlimit (0, 5, wave)]);
    if (src == 1) return "SMP";
    if (src == 2) return w + "+SMP";
    return w;
}

AuraLook::AuraLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, ui::outline);
    setColour (juce::Slider::thumbColourId, ui::accent);
    setColour (juce::Slider::trackColourId, ui::accent.withAlpha (0.6f));
    setColour (juce::Slider::backgroundColourId, ui::outline);
    setColour (juce::Slider::textBoxTextColourId, ui::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::ComboBox::backgroundColourId, ui::panel2);
    setColour (juce::ComboBox::outlineColourId, ui::outline);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::arrowColourId, ui::dim);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::accent.withAlpha (0.3f));
    setColour (juce::TextButton::buttonColourId, ui::panel2);
    setColour (juce::TextButton::buttonOnColourId, ui::accent.withAlpha (0.25f));
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::TextButton::textColourOnId, ui::accent);
    setColour (juce::ToggleButton::textColourId, ui::text);
    setColour (juce::ToggleButton::tickColourId, ui::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, ui::dim);
    setColour (juce::TabbedComponent::backgroundColourId, ui::panel);
    setColour (juce::TabbedComponent::outlineColourId, ui::outline);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, ui::outline);
    setColour (juce::TabbedButtonBar::tabTextColourId, ui::dim);
    setColour (juce::TabbedButtonBar::frontTextColourId, ui::accent);
}

void AuraLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                 float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    if (r < 4.f) return;
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float lw = std::max (2.f, r * 0.16f);
    const float ar = r - lw * 0.5f;

    juce::Path track;
    track.addCentredArc (cx, cy, ar, ar, 0.f, a0, a1, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc (cx, cy, ar, ar, 0.f, std::min (from, ang), std::max (from, ang), true);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (s.isEnabled() ? 1.f : 0.4f));
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = r * 0.62f;
    g.setColour (ui::panel2.brighter (0.15f));
    g.fillEllipse (cx - kr, cy - kr, kr * 2.f, kr * 2.f);
    g.setColour (ui::text);
    g.drawLine (cx + std::sin (ang) * kr * 0.25f, cy - std::cos (ang) * kr * 0.25f,
                cx + std::sin (ang) * kr * 0.9f,  cy - std::cos (ang) * kr * 0.9f, std::max (1.5f, r * 0.08f));
}

// ============================================================================
// Helpers
// ============================================================================
// Couleurs posees sur le slider lui-meme : sa zone de texte est creee avant d'avoir un parent
// (donc avant d'heriter du LookAndFeel), d'ou le cadre blanc sinon.
static void styleKnob (juce::Slider& s)
{
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, ui::dim);
    s.setColour (juce::Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.4f));
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
}

void KnobSet::add (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    styleKnob (k->slider);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.f));
    k->label.setColour (juce::Label::textColourId, ui::dim);
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
                                          area.getY() + (i / cols) * h, w, h).reduced (2);
        knobs[(size_t) i]->label.setBounds (cell.removeFromTop (16));
        knobs[(size_t) i]->slider.setBounds (cell);
    }
}

void Section::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (b, 6.f);
    g.setColour (ui::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.f, 1.f);
    g.setColour (colour);
    g.fillRoundedRectangle (juce::Rectangle<float> (10.f, 11.f, 3.f, 12.f), 1.5f);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (14.f, juce::Font::bold));
    g.drawText (title, 20, 6, getWidth() - 28, 22, juce::Justification::centredLeft);
}

ParamPanel::ParamPanel (APVTS_& s, const juce::String& t, juce::Colour c, int knobCols)
    : Section (t, c), apvts (s), knobs (*this, s), cols (knobCols)
{
    action.onClick = [this] { if (actionFn) actionFn(); };
}

void ParamPanel::addToggle (const juce::String& id, const juce::String& text)
{
    auto t = std::make_unique<ToggleBox>();
    t->init (*this, apvts, id, text);
    toggles.push_back (std::move (t));
}

void ParamPanel::addCombo (const juce::String& id, const juce::String& text, const juce::StringArray& items)
{
    auto c = std::make_unique<LabeledCombo>();
    c->init (*this, apvts, id, text, items);
    combos.push_back (std::move (c));
}

void ParamPanel::setAction (const juce::String& text, std::function<void()> fn)
{
    action.setButtonText (text);
    actionFn = std::move (fn);
    addAndMakeVisible (action);
}

void ParamPanel::resized()
{
    auto a = content();
    if (actionFn)
    {
        action.setBounds (a.removeFromBottom (28));
        a.removeFromBottom (8);
    }
    for (auto& t : toggles)
    {
        t->button.setBounds (a.removeFromTop (26));
        a.removeFromTop (2);
    }
    for (auto& c : combos)
    {
        auto r = a.removeFromTop (26);
        c->place (r, 60, std::min (140, r.getWidth()));
        a.removeFromTop (4);
    }
    a.removeFromTop (6);
    const int rowsN = (numKnobs + cols - 1) / std::max (1, cols);
    knobs.layout (a.removeFromTop (std::min (a.getHeight(), rowsN * 84)), cols);
}

// ============================================================================
// Visualisations
// ============================================================================
static void drawDisplayBg (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (juce::Colour (0xff0d0f13));
    g.fillRoundedRectangle (b, 4.f);
    g.setColour (ui::outline.withAlpha (0.6f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (b.getX() + b.getWidth() * float (i) / 8.f), b.getY(), b.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * float (i) / 4.f), b.getX(), b.getRight());
}

static void strokeAndFill (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> b, juce::Colour c)
{
    juce::Path f (p);
    f.lineTo (p.getCurrentPosition().x, b.getBottom());
    f.lineTo (b.getX(), b.getBottom());
    f.closeSubPath();
    g.setColour (c.withAlpha (0.15f));
    g.fillPath (f);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (2.f));
}

FilterView::FilterView (APVTS_& s, int layer, juce::Colour c)
    : cut (s.getRawParameterValue (lpId (layer, LP_Cutoff))),
      res (s.getRawParameterValue (lpId (layer, LP_Res))), colour (c)
{
    startTimerHz (15);
}

void FilterView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawDisplayBg (g, b);
    const float fc = cut->load(), q = std::max (0.3f, res->load());

    juce::Path p;
    const int steps = std::max (2, (int) b.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float t = float (i) / float (steps);
        const float f = 20.f * std::pow (1000.f, t);                // 20 Hz .. 20 kHz
        const float x = f / fc;
        const float mag = 1.f / std::sqrt ((1.f - x * x) * (1.f - x * x) + (x / q) * (x / q));
        const float db = juce::jlimit (-48.f, 24.f, 20.f * std::log10 (std::max (mag, 1e-6f)));
        const float py = b.getY() + (24.f - db) / 72.f * b.getHeight();
        const float px = b.getX() + t * b.getWidth();
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    strokeAndFill (g, p, b, colour);
}

EnvView::EnvView (APVTS_& s, int layer, int lpAtk, juce::Colour c) : colour (c)
{
    for (int i = 0; i < 4; ++i) p[i] = s.getRawParameterValue (lpId (layer, lpAtk + i));
    startTimerHz (15);
}

void EnvView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawDisplayBg (g, b);
    const auto r = b.reduced (6.f);

    // echelle en racine pour que les temps courts restent lisibles
    const float a = std::sqrt (p[0]->load()), d = std::sqrt (p[1]->load()), rl = std::sqrt (p[3]->load());
    const float s = p[2]->load();
    const float hold = 0.6f;
    const float total = a + d + hold + rl;
    auto X = [&] (float t) { return r.getX() + t / total * r.getWidth(); };
    auto Y = [&] (float v) { return r.getBottom() - v * r.getHeight(); };

    juce::Path path;
    path.startNewSubPath (X (0.f), Y (0.f));
    path.lineTo (X (a), Y (1.f));
    path.quadraticTo (X (a + d * 0.25f), Y (s + (1.f - s) * 0.1f), X (a + d), Y (s));
    path.lineTo (X (a + d + hold), Y (s));
    path.quadraticTo (X (a + d + hold + rl * 0.25f), Y (s * 0.1f), X (total), Y (0.f));
    strokeAndFill (g, path, r, colour);
}

LfoView::LfoView (APVTS_& s, int layer, int lpShape, juce::Colour c)
    : shape (s.getRawParameterValue (lpId (layer, lpShape))), colour (c)
{
    startTimerHz (10);
}

void LfoView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawDisplayBg (g, b);
    const auto r = b.reduced (6.f);
    const int sh = juce::roundToInt (shape->load());
    static const float sh8[8] = { 0.3f, -0.7f, 0.9f, 0.1f, -0.4f, 0.6f, -0.9f, 0.2f };

    juce::Path p;
    const int steps = std::max (2, (int) r.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float t = float (i) / float (steps) * 2.f;      // 2 cycles
        const float ph = t - std::floor (t);
        float v = 0.f;
        switch (sh)
        {
            case 0: v = std::sin (2.f * kPi * ph); break;
            case 1: v = 1.f - 4.f * std::abs (ph - 0.5f); break;
            case 2: v = 2.f * ph - 1.f; break;
            case 3: v = ph < 0.5f ? 1.f : -1.f; break;
            default: v = sh8[juce::jlimit (0, 7, int (t * 4.f))]; break;
        }
        const float px = r.getX() + float (i) / float (steps) * r.getWidth();
        const float py = r.getCentreY() - v * r.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (2.f));
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
    drawDisplayBg (g, getLocalBounds().toFloat());
    const auto a = area();

    juce::Path path;
    const int steps = 300;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = float (i) / float (steps);
        const auto pt = toPx (t, shape.eval (t));
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    strokeAndFill (g, path, a, ui::accent);

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
        g.setColour (ui::accent);
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

// ============================================================================
// Page LAYER
// ============================================================================
static void setParam (APVTS_& s, const juce::String& id, float value)
{
    if (auto* p = s.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

static void styleToggleButton (juce::TextButton& b, juce::Colour c)
{
    b.setClickingTogglesState (false);
    b.setColour (juce::TextButton::buttonOnColourId, c.withAlpha (0.35f));
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

// ---------------- AnalogPanel ----------------
static const int kAnalogWaves[4] = { 0, 1, 4, 5 };          // Saw, Square, Triangle, Sine (valeurs du param)

AnalogPanel::AnalogPanel (AuraProcessor& p, int layerIndex) : proc (p), layer (layerIndex), knobs (*this, p.apvts)
{
    static const char* const names[4] = { "SAW", "SQUARE", "TRIANGLE", "SINE" };
    for (int i = 0; i < 4; ++i)
    {
        waveBtn[i].setButtonText (names[i]);
        styleToggleButton (waveBtn[i], ui::layerColour (layer));
        waveBtn[i].onClick = [this, i] { setParam (proc.apvts, lpId (layer, LP_Wave), (float) kAnalogWaves[i]); };
        addAndMakeVisible (waveBtn[i]);
    }
    knobs.add (lpId (layer, LP_Unison), "Unison");
    knobs.add (lpId (layer, LP_Detune), "Detune");
    waveP = p.apvts.getRawParameterValue (lpId (layer, LP_Wave));
    uniP  = p.apvts.getRawParameterValue (lpId (layer, LP_Unison));
    detP  = p.apvts.getRawParameterValue (lpId (layer, LP_Detune));
    startTimerHz (10);
}

void AnalogPanel::timerCallback()
{
    if (! isShowing()) return;
    const int w = juce::roundToInt (waveP->load());
    for (int i = 0; i < 4; ++i) waveBtn[i].setToggleState (w == kAnalogWaves[i], juce::dontSendNotification);
    repaint();
}

void AnalogPanel::resized()
{
    auto a = getLocalBounds();
    auto r = a.removeFromTop (24);
    const int bw = r.getWidth() / 4;
    for (auto& b : waveBtn) b.setBounds (r.removeFromLeft (bw).reduced (2, 0));
    a.removeFromTop (6);
    knobs.layout (a.removeFromRight (std::min (150, a.getWidth() / 3)), 1);
    a.removeFromRight (6);
    viewArea = a;
}

void AnalogPanel::paint (juce::Graphics& g)
{
    const auto b = viewArea.toFloat();
    drawDisplayBg (g, b);
    auto r = b.reduced (8.f);
    const int w = juce::roundToInt (waveP->load());
    const int u = juce::jlimit (1, 7, juce::roundToInt (uniP->load()));
    const float det = detP->load();
    const auto col = ui::layerColour (layer);

    auto wave = [w] (float ph)
    {
        ph -= std::floor (ph);
        switch (w)
        {
            case 1:  return ph < 0.5f ? 0.8f : -0.8f;
            case 4:  return (1.f - 4.f * std::abs (ph - 0.5f)) * 0.9f;
            case 5:  return std::sin (2.f * kPi * ph) * 0.9f;
            default: return (2.f * ph - 1.f) * 0.9f;
        }
    };

    // voix d'unison decalees (visualise le "Saw+" / supersaw)
    for (int v = u - 1; v >= 0; --v)
    {
        const float o = (u > 1) ? (2.f * float (v) / float (u - 1) - 1.f) : 0.f;
        juce::Path p;
        const int steps = std::max (2, (int) r.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float t = float (i) / float (steps) * 2.f;              // 2 cycles
            const float y = r.getCentreY() - wave (t * (1.f + o * det * 0.06f) + o * det * 0.15f) * r.getHeight() * 0.45f;
            const float x = r.getX() + float (i) / float (steps) * r.getWidth();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        const bool centre = (u == 1) || std::abs (o) < 0.01f || (u % 2 == 0 && v == u / 2);
        g.setColour (col.withAlpha (centre ? 1.f : 0.3f));
        g.strokePath (p, juce::PathStrokeType (centre ? 2.f : 1.f));
    }
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText (u > 1 ? juce::String (u) + " voices unison" : juce::String ("mono"),
                r.removeFromBottom (14.f), juce::Justification::bottomRight);
}

// ---------------- WavetablePanel ----------------
WavetablePanel::WavetablePanel (AuraProcessor& p, int layerIndex) : proc (p), layer (layerIndex), knobs (*this, p.apvts)
{
    knobs.add (lpId (layer, LP_WtPos),  "Position");
    knobs.add (lpId (layer, LP_Unison), "Unison");
    knobs.add (lpId (layer, LP_Detune), "Detune");
    posP = p.apvts.getRawParameterValue (lpId (layer, LP_WtPos));
    startTimerHz (20);
}

void WavetablePanel::resized()
{
    auto a = getLocalBounds();
    knobs.layout (a.removeFromRight (std::min (150, a.getWidth() / 3)), 1);
    a.removeFromRight (6);
    viewArea = a;
}

void WavetablePanel::paint (juce::Graphics& g)
{
    const auto b = viewArea.toFloat();
    g.setColour (juce::Colour (0xff0d0f13));
    g.fillRoundedRectangle (b, 4.f);

    const auto& wt = proc.getWavetables();
    const int nf = WavetableBank::kFrames;
    const float framePos = posP->load() * float (nf - 1);
    const auto col = ui::layerColour (layer);

    // vue pseudo-3D : chaque frame decalee en haut a droite
    const float dx = b.getWidth() * 0.035f, dy = b.getHeight() * 0.05f;
    const auto base = juce::Rectangle<float> (b.getX() + 10.f, b.getY() + dy * float (nf - 1) + 8.f,
                                              b.getWidth() - 20.f - dx * float (nf - 1),
                                              b.getHeight() - dy * float (nf - 1) - 24.f);
    auto framePath = [&] (float fp, float offX, float offY)
    {
        juce::Path p;
        const int steps = 160;
        for (int i = 0; i <= steps; ++i)
        {
            const float ph = float (i) / float (steps);
            const float v = wt.read (0, fp, std::min (ph, 0.9999f));
            const float x = base.getX() + offX + ph * base.getWidth();
            const float y = base.getCentreY() - offY - v * base.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    for (int f = nf - 1; f >= 0; --f)
    {
        const float a = 0.12f + 0.25f * (1.f - std::min (1.f, std::abs (float (f) - framePos)));
        g.setColour (col.withAlpha (a));
        g.strokePath (framePath ((float) f, dx * float (f), dy * float (f)), juce::PathStrokeType (1.f));
    }
    g.setColour (col);
    g.strokePath (framePath (framePos, dx * framePos, dy * framePos), juce::PathStrokeType (2.2f));

    static const char* const names[8] = { "Sine", "Triangle", "Saw", "Square", "Pulse 25%", "Formant", "Organ", "Buzz" };
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (12.f));
    g.drawText ("Frame " + juce::String (framePos + 1.f, 1) + " / " + juce::String (nf) + "  -  "
                    + names[juce::jlimit (0, nf - 1, juce::roundToInt (framePos))],
                b.reduced (8.f, 4.f).removeFromBottom (16.f), juce::Justification::centredLeft);
}

// ---------------- FmPanel ----------------
FmPanel::FmPanel (AuraProcessor& p, int layerIndex) : layer (layerIndex), knobs (*this, p.apvts)
{
    algo.init (*this, p.apvts, lpId (layer, LP_FmAlgo), "Algo", { "2-Op", "3-Op Stack", "3-Op Parallel" });
    knobs.add (lpId (layer, LP_FmR1),  "Ratio 1");
    knobs.add (lpId (layer, LP_FmR2),  "Ratio 2");
    knobs.add (lpId (layer, LP_FmIdx), "Index");
    knobs.add (lpId (layer, LP_FmFb),  "Feedback");
    algoP = p.apvts.getRawParameterValue (lpId (layer, LP_FmAlgo));
    r1P = p.apvts.getRawParameterValue (lpId (layer, LP_FmR1));
    r2P = p.apvts.getRawParameterValue (lpId (layer, LP_FmR2));
    startTimerHz (5);
}

void FmPanel::resized()
{
    auto a = getLocalBounds();
    auto r = a.removeFromTop (24);
    algo.place (r, 40, std::min (150, r.getWidth() - 48));
    a.removeFromTop (6);
    knobs.layout (a.removeFromRight (std::min (220, a.getWidth() / 2)), 2);
    a.removeFromRight (6);
    viewArea = a;
}

void FmPanel::paint (juce::Graphics& g)
{
    const auto b = viewArea.toFloat();
    g.setColour (juce::Colour (0xff0d0f13));
    g.fillRoundedRectangle (b, 4.f);
    const auto col = ui::layerColour (layer);
    const int al = juce::roundToInt (algoP->load());

    const float bw = std::min (90.f, b.getWidth() * 0.3f), bh = 30.f;
    auto box = [&] (juce::Rectangle<float> r, const juce::String& t, bool carrier)
    {
        g.setColour (carrier ? col.withAlpha (0.35f) : ui::panel2);
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (carrier ? col : ui::outline.brighter (0.3f));
        g.drawRoundedRectangle (r, 4.f, 1.5f);
        g.setColour (ui::text);
        g.setFont (juce::FontOptions (12.f));
        g.drawText (t, r, juce::Justification::centred);
    };
    auto arrow = [&] (juce::Point<float> a, juce::Point<float> c)
    {
        juce::Path p;
        p.addArrow ({ a, c }, 1.5f, 8.f, 8.f);
        g.setColour (ui::dim);
        g.fillPath (p);
    };

    const float cx = b.getCentreX();
    const auto carrier = juce::Rectangle<float> (cx - bw / 2, b.getBottom() - bh - 14.f, bw, bh);
    const juce::String m1 = "Op 1  x" + juce::String (r1P->load(), 2);
    const juce::String m2 = "Op 2  x" + juce::String (r2P->load(), 2);

    if (al == 1)          // pile : Op2 -> Op1 -> porteuse
    {
        const auto o1 = carrier.translated (0.f, -(bh + 18.f));
        const auto o2 = o1.translated (0.f, -(bh + 18.f));
        box (o2, m2, false); box (o1, m1, false);
        arrow ({ cx, o2.getBottom() }, { cx, o1.getY() });
        arrow ({ cx, o1.getBottom() }, { cx, carrier.getY() });
    }
    else if (al == 2)     // parallele : Op1 + Op2 -> porteuse
    {
        const auto o1 = carrier.translated (-(bw * 0.6f), -(bh + 30.f));
        const auto o2 = carrier.translated (bw * 0.6f, -(bh + 30.f));
        box (o1, m1, false); box (o2, m2, false);
        arrow ({ o1.getCentreX(), o1.getBottom() }, { cx - 10.f, carrier.getY() });
        arrow ({ o2.getCentreX(), o2.getBottom() }, { cx + 10.f, carrier.getY() });
    }
    else                  // 2-Op : Op1 -> porteuse
    {
        const auto o1 = carrier.translated (0.f, -(bh + 26.f));
        box (o1, m1, false);
        arrow ({ cx, o1.getBottom() }, { cx, carrier.getY() });
    }
    box (carrier, "Carrier", true);
}

// ---------------- SamplePanel ----------------
SamplePanel::SamplePanel (AuraProcessor& p, int layerIndex) : proc (p), layer (layerIndex)
{
    oneShot.init (*this, p.apvts, lpId (layer, LP_OneShot), "One-shot");
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Select samples", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<SamplePanel> safe (this);
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
    statusLabel.setColour (juce::Label::textColourId, ui::dim);
    statusLabel.setFont (juce::FontOptions (12.f));
    addAndMakeVisible (statusLabel);
    startTimerHz (4);
}

void SamplePanel::loadFiles (const juce::Array<juce::File>& files)
{
    proc.loadSamples (layer, files);
    if (juce::roundToInt (proc.apvts.getRawParameterValue (lpId (layer, LP_Source))->load()) == 0)
        setParam (proc.apvts, lpId (layer, LP_Source), 1.f);
}

void SamplePanel::timerCallback()
{
    statusLabel.setText (proc.getStatusText (layer), juce::dontSendNotification);
    int v = 0;
    auto b = proc.getUiBank (layer, v);
    if (v != bankVersion)
    {
        bankVersion = v;
        bank = b;
        rebuild();
        repaint();
    }
}

void SamplePanel::rebuild()
{
    mins.clear(); maxs.clear(); info.clear();
    loopS = loopE = -1.f;
    if (bank == nullptr || bank->zones.empty()) return;

    const Zone* z = &bank->zones.front();
    for (const auto& zz : bank->zones)
        if (std::abs (zz.rootKey - 60) < std::abs (z->rootKey - 60)) z = &zz;

    const int len = z->data.getNumSamples();
    if (len <= 0) return;
    const int nb = 512;
    mins.assign (nb, 0.f);
    maxs.assign (nb, 0.f);
    const float* l = z->data.getReadPointer (0);
    const float* r = z->data.getReadPointer (1);
    for (int i = 0; i < nb; ++i)
    {
        const int s0 = int ((long long) i * len / nb), s1 = std::max (s0 + 1, int ((long long) (i + 1) * len / nb));
        float mn = 1.f, mx = -1.f;
        for (int s = s0; s < s1 && s < len; ++s)
        {
            const float v = 0.5f * (l[s] + r[s]);
            mn = std::min (mn, v);
            mx = std::max (mx, v);
        }
        mins[(size_t) i] = mn;
        maxs[(size_t) i] = mx;
    }
    if (z->hasLoop())
    {
        loopS = float (z->loopStart) / float (len);
        loopE = float (z->loopEnd) / float (len);
    }
    info = juce::String ((int) bank->zones.size()) + " zone(s)  -  shown: " + z->name + "  (root "
         + juce::MidiMessage::getMidiNoteName (z->rootKey, true, true, 4) + ", "
         + juce::String (double (len) / z->srcRate, 2) + " s)";
}

void SamplePanel::resized()
{
    auto a = getLocalBounds();
    auto r = a.removeFromTop (24);
    loadButton.setBounds (r.removeFromLeft (130));
    r.removeFromLeft (8);
    oneShot.button.setBounds (r.removeFromLeft (95));
    r.removeFromLeft (8);
    statusLabel.setBounds (r);
    a.removeFromTop (6);
    viewArea = a;
}

void SamplePanel::paint (juce::Graphics& g)
{
    const auto b = viewArea.toFloat();
    drawDisplayBg (g, b);
    const auto col = ui::layerColour (layer);

    if (mins.empty())
    {
        g.setColour (ui::dim);
        g.setFont (juce::FontOptions (13.f));
        g.drawText ("Drop samples here or click 'Load samples...'", b, juce::Justification::centred);
        return;
    }

    const auto r = b.reduced (6.f).withTrimmedBottom (16.f);
    if (loopS >= 0.f)
    {
        g.setColour (col.withAlpha (0.12f));
        g.fillRect (r.getX() + loopS * r.getWidth(), r.getY(), (loopE - loopS) * r.getWidth(), r.getHeight());
        g.setColour (col.withAlpha (0.7f));
        g.drawVerticalLine ((int) (r.getX() + loopS * r.getWidth()), r.getY(), r.getBottom());
        g.drawVerticalLine ((int) (r.getX() + loopE * r.getWidth()), r.getY(), r.getBottom());
    }
    g.setColour (col.withAlpha (0.85f));
    const int nb = (int) mins.size();
    const float bwid = r.getWidth() / float (nb);
    for (int i = 0; i < nb; ++i)
    {
        const float y0 = r.getCentreY() - maxs[(size_t) i] * r.getHeight() * 0.5f;
        const float y1 = r.getCentreY() - mins[(size_t) i] * r.getHeight() * 0.5f;
        g.fillRect (r.getX() + float (i) * bwid, y0, std::max (1.f, bwid), std::max (1.f, y1 - y0));
    }
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText (info, b.reduced (8.f, 2.f).removeFromBottom (16.f), juce::Justification::centredLeft);
}

// ---------------- OscSection ----------------
OscSection::OscSection (AuraProcessor& p, int layerIndex)
    : Section ("Oscillator", ui::layerColour (layerIndex)), proc (p), layer (layerIndex),
      analog (p, layerIndex), wavetable (p, layerIndex), fm (p, layerIndex), sample (p, layerIndex)
{
    source.init (*this, p.apvts, lpId (layer, LP_Source), "Source", { "Synth", "Sample", "Synth+Sample" });

    static const char* const names[kNumTypes] = { "ANALOG", "WAVETABLE", "FM" };
    for (int i = 0; i < kNumTypes; ++i)
    {
        typeBtn[i].setButtonText (names[i]);
        styleToggleButton (typeBtn[i], ui::layerColour (layer));
        typeBtn[i].onClick = [this, i]
        {
            const float w = (i == TypeWavetable) ? 2.f : (i == TypeFm ? 3.f : (float) lastAnalogWave);
            setParam (proc.apvts, lpId (layer, LP_Wave), w);
        };
        addAndMakeVisible (typeBtn[i]);
    }
    addChildComponent (analog);
    addChildComponent (wavetable);
    addChildComponent (fm);
    addChildComponent (sample);
    startTimerHz (8);
    timerCallback();
}

int OscSection::currentSource() const
{
    return juce::roundToInt (proc.apvts.getRawParameterValue (lpId (layer, LP_Source))->load());
}

int OscSection::currentType() const
{
    const int w = juce::roundToInt (proc.apvts.getRawParameterValue (lpId (layer, LP_Wave))->load());
    return w == 2 ? TypeWavetable : (w == 3 ? TypeFm : TypeAnalog);
}

void OscSection::timerCallback()
{
    const int w = juce::roundToInt (proc.apvts.getRawParameterValue (lpId (layer, LP_Wave))->load());
    if (w != 2 && w != 3) lastAnalogWave = w;

    const int src = currentSource(), type = currentType();
    if (src == lastSrc && type == lastType) return;
    lastSrc = src;
    lastType = type;

    const bool synth = src != 1;
    for (int i = 0; i < kNumTypes; ++i)
    {
        typeBtn[i].setToggleState (i == type, juce::dontSendNotification);
        typeBtn[i].setVisible (synth);
    }
    analog.setVisible (synth && type == TypeAnalog);
    wavetable.setVisible (synth && type == TypeWavetable);
    fm.setVisible (synth && type == TypeFm);
    sample.setVisible (src >= 1);
    resized();
}

void OscSection::resized()
{
    auto a = content();
    auto r = a.removeFromTop (26);
    source.place (r, 52, 130);
    r.removeFromLeft (10);
    for (auto& b : typeBtn) b.setBounds (r.removeFromLeft (96).reduced (2, 0));
    a.removeFromTop (8);

    const int src = lastSrc;
    if (src == 2)
    {
        auto left = a.removeFromLeft (a.getWidth() * 55 / 100);
        a.removeFromLeft (10);
        for (juce::Component* c : { (juce::Component*) &analog, (juce::Component*) &wavetable, (juce::Component*) &fm })
            c->setBounds (left);
        sample.setBounds (a);
    }
    else
    {
        for (juce::Component* c : { (juce::Component*) &analog, (juce::Component*) &wavetable,
                                    (juce::Component*) &fm, (juce::Component*) &sample })
            c->setBounds (a);
    }
}

FilterSection::FilterSection (AuraProcessor& p, int layer)
    : Section ("Filter (LP 12 dB)", ui::layerColour (layer)),
      view (p.apvts, layer, ui::layerColour (layer)), knobs (*this, p.apvts)
{
    addAndMakeVisible (view);
    knobs.add (lpId (layer, LP_Cutoff), "Cutoff");
    knobs.add (lpId (layer, LP_Res),    "Resonance");
}

void FilterSection::resized()
{
    auto a = content();
    view.setBounds (a.removeFromTop (a.getHeight() * 50 / 100));
    a.removeFromTop (4);
    knobs.layout (a, 2);
}

AmpSection::AmpSection (AuraProcessor& p, int layer)
    : Section ("Amp", ui::layerColour (layer)),
      view (p.apvts, layer, LP_Atk, ui::layerColour (layer)), knobs (*this, p.apvts)
{
    addAndMakeVisible (view);
    knobs.add (lpId (layer, LP_Atk),   "Attack");
    knobs.add (lpId (layer, LP_Dec),   "Decay");
    knobs.add (lpId (layer, LP_Sus),   "Sustain");
    knobs.add (lpId (layer, LP_Rel),   "Release");
    knobs.add (lpId (layer, LP_Level), "Level");
    knobs.add (lpId (layer, LP_Pan),   "Pan");
}

void AmpSection::resized()
{
    auto a = content();
    view.setBounds (a.removeFromTop (a.getHeight() * 38 / 100));
    a.removeFromTop (4);
    knobs.layout (a, 3);
}

EnvPane::EnvPane (AuraProcessor& p, int layer)
    : view (p.apvts, layer, LP_MAtk, ui::layerColour (layer)), knobs (*this, p.apvts)
{
    addAndMakeVisible (view);
    knobs.add (lpId (layer, LP_MAtk), "Attack");
    knobs.add (lpId (layer, LP_MDec), "Decay");
    knobs.add (lpId (layer, LP_MSus), "Sustain");
    knobs.add (lpId (layer, LP_MRel), "Release");
}

void EnvPane::resized()
{
    auto a = getLocalBounds().reduced (6);
    view.setBounds (a.removeFromLeft (a.getWidth() * 60 / 100));
    a.removeFromLeft (6);
    knobs.layout (a, 2);
}

LfoPane::LfoPane (AuraProcessor& p, int layer, int idx)
    : view (p.apvts, layer, idx == 0 ? LP_Lfo1Shape : LP_Lfo2Shape, ui::layerColour (layer)),
      knobs (*this, p.apvts)
{
    addAndMakeVisible (view);
    shape.init (*this, p.apvts, lpId (layer, idx == 0 ? LP_Lfo1Shape : LP_Lfo2Shape), "Shape",
                { "Sine", "Triangle", "Saw", "Square", "S&H" });
    knobs.add (lpId (layer, idx == 0 ? LP_Lfo1Rate : LP_Lfo2Rate), "Rate (Hz)");
}

void LfoPane::resized()
{
    auto a = getLocalBounds().reduced (6);
    view.setBounds (a.removeFromLeft (a.getWidth() * 60 / 100));
    a.removeFromLeft (6);
    auto r = a.removeFromTop (26);
    shape.place (r, 48, std::max (60, r.getWidth() - 56));
    a.removeFromTop (6);
    knobs.layout (a.removeFromTop (std::min (a.getHeight(), 110)), 1);
}

MsegPane::MsegPane (AuraProcessor& p, int layerIndex, int index)
    : proc (p), layer (layerIndex), idx (index), knobs (*this, p.apvts)
{
    addAndMakeVisible (editor);
    seenVersion = p.getStateVersion();
    editor.setShape (p.getMseg (layer, idx));
    editor.onChange = [this] (const MsegShape& s) { proc.setMseg (layer, idx, s); };
    loop.init (*this, p.apvts, lpId (layer, idx == 0 ? LP_Mseg1Loop : LP_Mseg2Loop), "Loop");
    knobs.add (lpId (layer, idx == 0 ? LP_Mseg1Time : LP_Mseg2Time), "Time (s)");
    startTimerHz (4);

    hint.setText ("Double-click: add / delete point. Drag diamond: curve.", juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, ui::dim);
    hint.setFont (juce::FontOptions (11.f));
    addAndMakeVisible (hint);
}

void MsegPane::timerCallback()
{
    const int v = proc.getStateVersion();
    if (v == seenVersion) return;
    seenVersion = v;
    editor.setShape (proc.getMseg (layer, idx));
}

void MsegPane::resized()
{
    auto a = getLocalBounds().reduced (6);
    hint.setBounds (a.removeFromBottom (16));
    auto side = a.removeFromRight (110);
    a.removeFromRight (6);
    editor.setBounds (a);
    loop.button.setBounds (side.removeFromTop (26));
    side.removeFromTop (6);
    knobs.layout (side.removeFromTop (std::min (side.getHeight(), 110)), 1);
}

ModSourcesSection::ModSourcesSection (AuraProcessor& p, int layer)
    : Section ("Modulation sources", ui::layerColour (layer)),
      env (p, layer), lfo1 (p, layer, 0), lfo2 (p, layer, 1),
      mseg1 (p, layer, 0), mseg2 (p, layer, 1)
{
    const auto bgc = ui::panel;
    tabs.addTab ("MOD ENV", bgc, &env, false);
    tabs.addTab ("LFO 1",   bgc, &lfo1, false);
    tabs.addTab ("LFO 2",   bgc, &lfo2, false);
    tabs.addTab ("MSEG 1",  bgc, &mseg1, false);
    tabs.addTab ("MSEG 2",  bgc, &mseg2, false);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);
}

void ModSourcesSection::resized()
{
    tabs.setBounds (content());
}

MatrixSection::MatrixSection (AuraProcessor& p, int layer)
    : Section ("Mod matrix", ui::layerColour (layer))
{
    const auto srcItems = namesToArray (kSrcNames, kNumSrc);
    const auto dstItems = namesToArray (kDstNames, kNumDst);

    for (auto* l : { &hSrc, &hDst, &hAmt })
    {
        l->setColour (juce::Label::textColourId, ui::dim);
        l->setFont (juce::FontOptions (11.f));
        addAndMakeVisible (*l);
    }
    hSrc.setText ("Source", juce::dontSendNotification);
    hDst.setText ("Destination", juce::dontSendNotification);
    hAmt.setText ("Amount", juce::dontSendNotification);

    for (int i = 0; i < kSlots; ++i)
    {
        auto r = std::make_unique<Row>();
        r->num.setText (juce::String (i + 1), juce::dontSendNotification);
        r->num.setJustificationType (juce::Justification::centred);
        r->num.setColour (juce::Label::textColourId, ui::dim);
        r->src.addItemList (srcItems, 1);
        r->dst.addItemList (dstItems, 1);
        r->amt.setSliderStyle (juce::Slider::LinearBar);
        r->amt.setColour (juce::Slider::trackColourId, ui::layerColour (layer).withAlpha (0.5f));
        r->amt.setColour (juce::Slider::textBoxTextColourId, ui::text);
        addAndMakeVisible (r->num);
        addAndMakeVisible (r->src);
        addAndMakeVisible (r->dst);
        addAndMakeVisible (r->amt);
        r->aS = std::make_unique<APVTS_::ComboBoxAttachment> (p.apvts, lpId (layer, lpSlot (i, 0)), r->src);
        r->aD = std::make_unique<APVTS_::ComboBoxAttachment> (p.apvts, lpId (layer, lpSlot (i, 1)), r->dst);
        r->aA = std::make_unique<APVTS_::SliderAttachment> (p.apvts, lpId (layer, lpSlot (i, 2)), r->amt);
        rows.push_back (std::move (r));
    }
}

void MatrixSection::resized()
{
    auto a = content();
    const int numW = 20;
    const int colW = (a.getWidth() - numW - 12) * 35 / 100;
    {
        auto h = a.removeFromTop (16);
        h.removeFromLeft (numW);
        hSrc.setBounds (h.removeFromLeft (colW + 6));
        hDst.setBounds (h.removeFromLeft (colW + 6));
        hAmt.setBounds (h);
    }
    const int rowH = std::min (28, a.getHeight() / std::max (1, (int) rows.size()));
    for (auto& r : rows)
    {
        auto row = a.removeFromTop (rowH).reduced (0, 2);
        r->num.setBounds (row.removeFromLeft (numW));
        r->src.setBounds (row.removeFromLeft (colW));
        row.removeFromLeft (6);
        r->dst.setBounds (row.removeFromLeft (colW));
        row.removeFromLeft (6);
        r->amt.setBounds (row);
    }
}

LayerEditor::LayerEditor (AuraProcessor& p, int layer)
    : osc (p, layer), filter (p, layer), amp (p, layer), mods (p, layer), matrix (p, layer)
{
    for (auto* c : std::initializer_list<juce::Component*> { &osc, &filter, &amp, &mods, &matrix })
        addAndMakeVisible (c);
}

void LayerEditor::resized()
{
    auto a = getLocalBounds();
    auto top = a.removeFromTop (a.getHeight() * 48 / 100);

    osc.setBounds (top.removeFromLeft (top.getWidth() * 50 / 100).reduced (4));
    filter.setBounds (top.removeFromLeft (top.getWidth() * 40 / 100).reduced (4));
    amp.setBounds (top.reduced (4));

    mods.setBounds (a.removeFromLeft (a.getWidth() * 58 / 100).reduced (4));
    matrix.setBounds (a.reduced (4));
}

// ============================================================================
// Piano roll
// ============================================================================
static constexpr float kKeyW = 52.f, kVelH = 46.f;

PianoRoll::PianoRoll (AuraProcessor& p, int layerIndex, int k) : proc (p), layer (layerIndex), kind (k)
{
    lenP = p.apvts.getRawParameterValue (lpId (layer, isArp() ? LP_ArpLen : LP_SeqLen));
    topRow = isArp() ? visibleRows() - 1 : 12;
    refresh();
    startTimerHz (30);
}

void PianoRoll::refresh()
{
    pat = proc.getPattern (layer, kind);
    repaint();
}

void PianoRoll::commit()
{
    proc.setPattern (layer, kind, pat);
    repaint();
}

int PianoRoll::lengthSteps() const
{
    return juce::jlimit (1, kMaxPatSteps, juce::roundToInt (lenP->load()));
}

juce::Rectangle<float> PianoRoll::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (kKeyW).withTrimmedBottom (kVelH + 6.f);
}

juce::Rectangle<float> PianoRoll::velArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (kKeyW).removeFromBottom (kVelH);
}

float PianoRoll::rowH() const { return gridArea().getHeight() / float (visibleRows()); }
float PianoRoll::stepW() const { return gridArea().getWidth() / float (lengthSteps()); }
float PianoRoll::rowY (int row) const { return gridArea().getY() + float (topRow - row) * rowH(); }

int PianoRoll::rowAt (float y) const
{
    return juce::jlimit (minRow(), maxRow(), topRow - (int) std::floor ((y - gridArea().getY()) / rowH()));
}

int PianoRoll::stepAt (float x) const
{
    return juce::jlimit (0, lengthSteps() - 1, (int) std::floor ((x - gridArea().getX()) / stepW()));
}

juce::Rectangle<float> PianoRoll::noteRect (const PatNote& n) const
{
    const auto g = gridArea();
    return { g.getX() + float (n.start) * stepW(), rowY (n.row), float (n.len) * stepW(), rowH() };
}

int PianoRoll::noteAt (juce::Point<float> p) const
{
    for (int i = pat.n - 1; i >= 0; --i)
        if (noteRect (pat.notes[i]).contains (p)) return i;
    return -1;
}

juce::String PianoRoll::rowLabel (int row) const
{
    if (isArp()) return "Note " + juce::String (row + 1);
    if (row == 0) return "Root";
    return (row > 0 ? "+" : "") + juce::String (row);
}

void PianoRoll::timerCallback()
{
    if (proc.getStateVersion() != seenVersion)
    {
        seenVersion = proc.getStateVersion();
        refresh();
    }
    if (! isShowing()) return;
    const int st = proc.getPlayStep (layer, kind);
    if (st != lastPlay || ++tick >= 6)
    {
        lastPlay = st;
        tick = 0;
        repaint();
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f13));
    const auto grid = gridArea();
    const auto col = ui::layerColour (layer);
    const int len = lengthSteps();
    const float rh = rowH(), sw = stepW();

    // lignes (fond facon clavier) + etiquettes
    g.setFont (juce::FontOptions (std::min (12.f, rh * 0.8f)));
    for (int i = 0; i < visibleRows(); ++i)
    {
        const int row = topRow - i;
        if (row < minRow()) break;
        const float y = grid.getY() + float (i) * rh;
        const int pc = ((row % 12) + 12) % 12;
        const bool dark = isArp() ? (row / 4) % 2 == 1 : (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        g.setColour (dark ? juce::Colour (0xff12151b) : juce::Colour (0xff171a21));
        g.fillRect (grid.getX(), y, grid.getWidth(), rh);
        if (! isArp() && row == 0)
        {
            g.setColour (col.withAlpha (0.10f));
            g.fillRect (grid.getX(), y, grid.getWidth(), rh);
        }
        g.setColour (dark ? ui::panel : ui::panel2);
        g.fillRect (0.f, y, kKeyW - 2.f, rh - 1.f);
        g.setColour ((! isArp() && row == 0) ? col : ui::dim);
        g.drawText (rowLabel (row), juce::Rectangle<float> (4.f, y, kKeyW - 8.f, rh), juce::Justification::centredLeft);
        g.setColour (ui::outline.withAlpha (0.5f));
        g.drawHorizontalLine ((int) (y + rh), grid.getX(), grid.getRight());
    }

    // colonnes, temps forts, tete de lecture
    const int cur = proc.getPlayStep (layer, kind);
    for (int s = 0; s <= len; ++s)
    {
        const float x = grid.getX() + float (s) * sw;
        if (s == cur)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (x, grid.getY(), sw, grid.getHeight());
        }
        g.setColour (s % 4 == 0 ? ui::outline.brighter (0.2f) : ui::outline.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, grid.getY(), grid.getBottom());
    }

    // notes
    for (int i = 0; i < pat.n; ++i)
    {
        const auto& n = pat.notes[i];
        if (n.start >= len || n.row > topRow || n.row <= topRow - visibleRows()) continue;
        auto r = noteRect (n).reduced (1.f, 1.f);
        r.setRight (std::min (r.getRight(), grid.getRight()));
        g.setColour (col.withAlpha (0.35f + 0.65f * float (n.vel) / 127.f));
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (i == dragNote ? juce::Colours::white : col.brighter (0.4f));
        g.drawRoundedRectangle (r, 3.f, i == dragNote ? 1.5f : 1.f);
    }
    // notes hors de la vue : indicateurs
    int above = 0, below = 0;
    for (int i = 0; i < pat.n; ++i)
    {
        if (pat.notes[i].row > topRow) ++above;
        else if (pat.notes[i].row <= topRow - visibleRows()) ++below;
    }
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    if (above > 0) g.drawText (juce::String (above) + " note(s) above - scroll", grid.reduced (6.f, 2.f), juce::Justification::topRight);
    if (below > 0) g.drawText (juce::String (below) + " note(s) below - scroll", grid.reduced (6.f, 2.f), juce::Justification::bottomRight);

    // ligne de velocite
    const auto va = velArea();
    g.setColour (ui::panel);
    g.fillRect (va);
    g.setColour (ui::dim);
    g.drawText ("Vel", juce::Rectangle<float> (4.f, va.getY(), kKeyW - 8.f, va.getHeight()), juce::Justification::centredLeft);
    for (int i = 0; i < pat.n; ++i)
    {
        const auto& n = pat.notes[i];
        if (n.start >= len) continue;
        const float h = va.getHeight() * float (n.vel) / 127.f;
        g.setColour (col.withAlpha (0.8f));
        g.fillRect (va.getX() + float (n.start) * sw + 2.f, va.getBottom() - h, std::max (3.f, sw * 0.4f), h);
    }
}

void PianoRoll::setVelAt (juce::Point<float> p)
{
    const auto va = velArea();
    const int s = stepAt (p.x);
    const int v = juce::jlimit (1, 127, juce::roundToInt ((va.getBottom() - p.y) / va.getHeight() * 127.f));
    for (int i = 0; i < pat.n; ++i)
        if (pat.notes[i].start == s) pat.notes[i].vel = (uint8_t) v;
    commit();
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (velArea().contains (p)) { drag = DragVel; setVelAt (p); return; }
    if (! gridArea().contains (p)) return;

    const int idx = noteAt (p);
    if (e.mods.isPopupMenu())
    {
        if (idx >= 0) { pat.remove (idx); dragNote = -1; commit(); }
        return;
    }
    if (idx >= 0)
    {
        dragNote = idx;
        const auto r = noteRect (pat.notes[idx]);
        if (p.x > r.getRight() - std::min (8.f, r.getWidth() * 0.3f)) drag = DragResize;
        else
        {
            drag = DragMove;
            grabStep = stepAt (p.x) - pat.notes[idx].start;
            grabRow = rowAt (p.y) - pat.notes[idx].row;
        }
    }
    else
    {
        // creation d'une note, puis le glisser regle sa longueur
        if (pat.add (stepAt (p.x), lastLen, rowAt (p.y), 100))
        {
            dragNote = pat.n - 1;
            drag = DragResize;
            commit();
        }
    }
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (drag == DragVel) { setVelAt (p); return; }
    if (dragNote < 0 || dragNote >= pat.n) return;
    auto& n = pat.notes[dragNote];
    if (drag == DragMove)
    {
        n.start = (int16_t) juce::jlimit (0, lengthSteps() - 1, stepAt (p.x) - grabStep);
        n.row = (int8_t) juce::jlimit (minRow(), maxRow(), rowAt (p.y) - grabRow);
    }
    else if (drag == DragResize)
    {
        const int endStep = (int) std::ceil ((p.x - gridArea().getX()) / stepW());
        n.len = (int16_t) juce::jlimit (1, kMaxPatSteps, endStep - n.start);
    }
    commit();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (drag == DragResize && dragNote >= 0 && dragNote < pat.n) lastLen = pat.notes[dragNote].len;
    drag = DragNone;
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int idx = noteAt (e.position);
    if (idx >= 0) { pat.remove (idx); dragNote = -1; commit(); }
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int dir = w.deltaY > 0.f ? 1 : (w.deltaY < 0.f ? -1 : 0);
    topRow = juce::jlimit (minRow() + visibleRows() - 1, maxRow(), topRow + dir);
    repaint();
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    const int idx = noteAt (e.position);
    if (idx >= 0 && e.position.x > noteRect (pat.notes[idx]).getRight() - 8.f)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (idx >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

// ============================================================================
// Page ARP / SEQ
// ============================================================================
SeqArpPage::SeqArpPage (AuraProcessor& p, int layerIndex)
    : proc (p), layer (layerIndex),
      arpPanel (p.apvts, "Arpeggiator", ui::layerColour (layerIndex), 3),
      seqPanel (p.apvts, "Sequencer", ui::layerColour (layerIndex), 3),
      patternBox ("Pattern   -   click: add  |  drag: move  |  right edge: length  |  double-click: delete  |  wheel: scroll",
                  ui::layerColour (layerIndex)),
      arpRoll (p, layerIndex, AuraProcessor::kPatArp),
      seqRoll (p, layerIndex, AuraProcessor::kPatSeq)
{
    const auto col = ui::layerColour (layer);
    for (auto* b : { &arpOn, &seqOn })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, col);
        b->setColour (juce::TextButton::textColourOnId, ui::bg);
        addAndMakeVisible (*b);
    }
    arpOnA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, lpId (layer, LP_Arp), arpOn);
    seqOnA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, lpId (layer, LP_SeqOn), seqOn);

    arpPanel.addToggle (lpId (layer, LP_ArpLatch), "Latch");
    arpPanel.addCombo (lpId (layer, LP_ArpRate), "Rate", kRateItems);
    arpPanel.addKnob (lpId (layer, LP_ArpLen), "Steps");
    arpPanel.addKnob (lpId (layer, LP_ArpGate), "Gate");
    arpPanel.addKnob (lpId (layer, LP_ArpSwing), "Swing");
    seqPanel.addCombo (lpId (layer, LP_SeqRate), "Rate", kRateItems);
    seqPanel.addKnob (lpId (layer, LP_SeqLen), "Steps");
    seqPanel.addKnob (lpId (layer, LP_SeqGate), "Gate");
    seqPanel.addKnob (lpId (layer, LP_SeqSwing), "Swing");
    addAndMakeVisible (arpPanel);
    addAndMakeVisible (seqPanel);

    static const char* const tabNames[2] = { "ARP PATTERN", "SEQ PATTERN" };
    for (int k = 0; k < 2; ++k)
    {
        editTab[k].setButtonText (tabNames[k]);
        styleToggleButton (editTab[k], col);
        editTab[k].onClick = [this, k] { showEditor (k); };
        addAndMakeVisible (editTab[k]);
    }

    genLabel.setText ("Generate:", juce::dontSendNotification);
    genLabel.setColour (juce::Label::textColourId, ui::dim);
    genLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (genLabel);
    for (int s = 2; s <= 8; ++s) genSpan.addItem (juce::String (s) + " notes", s);
    genSpan.setSelectedId (4, juce::dontSendNotification);
    addAndMakeVisible (genType);
    addAndMakeVisible (genSpan);

    genBtn.onClick    = [this] { generate(); };
    clearBtn.onClick  = [this] { proc.setPattern (layer, editing, Pattern()); roll (editing).refresh(); };
    importBtn.onClick = [this] { importMidi(); };
    exportBtn.onClick = [this] { exportMidi(); };
    for (auto* b : { &genBtn, &clearBtn, &importBtn, &exportBtn }) addAndMakeVisible (*b);

    addChildComponent (arpRoll);
    addChildComponent (seqRoll);
    addAndMakeVisible (patternBox);
    patternBox.toBack();                       // fond de la zone motif, sous les controles
    arpOn.toFront (false);                     // boutons ARP / SEQ dans l'en-tete des panneaux
    seqOn.toFront (false);
    showEditor (0);
}

void SeqArpPage::showEditor (int k)
{
    editing = k & 1;
    for (int i = 0; i < 2; ++i) editTab[i].setToggleState (i == editing, juce::dontSendNotification);
    arpRoll.setVisible (editing == 0);
    seqRoll.setVisible (editing == 1);
    roll (editing).refresh();

    genType.clear (juce::dontSendNotification);
    if (editing == 0) genType.addItemList ({ "Up", "Down", "Up/Down", "Random", "Chord" }, 1);
    else              genType.addItemList ({ "Root every step", "Octave bounce", "Root-5th-Oct", "Random +/-12" }, 1);
    genType.setSelectedId (1, juce::dontSendNotification);
    genSpan.setVisible (editing == 0);
}

double SeqArpPage::stepBeats (int k) const
{
    static const double kRates[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    const int r = juce::roundToInt (proc.apvts.getRawParameterValue (lpId (layer, k == 0 ? LP_ArpRate : LP_SeqRate))->load());
    return kRates[juce::jlimit (0, 5, r)];
}

void SeqArpPage::generate()
{
    const int k = editing;
    const int len = juce::roundToInt (proc.apvts.getRawParameterValue (lpId (layer, k == 0 ? LP_ArpLen : LP_SeqLen))->load());
    const int type = genType.getSelectedId() - 1;
    const int span = std::max (2, genSpan.getSelectedId());
    juce::Random rnd;
    Pattern p;

    for (int i = 0; i < len; ++i)
    {
        if (k == 0)
        {
            switch (type)
            {
                case 1:  p.add (i, 1, span - 1 - i % span, 100); break;
                case 2:
                {
                    const int m = std::max (1, 2 * span - 2), q = i % m;
                    p.add (i, 1, q < span ? q : m - q, 100);
                    break;
                }
                case 3:  p.add (i, 1, rnd.nextInt (span), 100); break;
                case 4:  for (int r = 0; r < span; ++r) p.add (i, 1, r, 100); break;
                default: p.add (i, 1, i % span, 100); break;
            }
        }
        else
        {
            static const int fifths[4] = { 0, 7, 12, 7 };
            const int vel = (i % 4 == 0) ? 120 : 85;
            switch (type)
            {
                case 1:  p.add (i, 1, (i % 2) * 12, vel); break;
                case 2:  p.add (i, 1, fifths[i % 4], vel); break;
                case 3:  p.add (i, 1, rnd.nextInt (25) - 12, vel); break;
                default: p.add (i, 1, 0, vel); break;
            }
        }
    }
    proc.setPattern (layer, k, p);
    roll (k).refresh();
}

void SeqArpPage::exportMidi()
{
    const int k = editing;
    const auto def = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("AURA_L" + juce::String (layer + 1) + (k == 0 ? "_arp.mid" : "_seq.mid"));
    chooser = std::make_unique<juce::FileChooser> ("Export pattern as MIDI", def, "*.mid");
    juce::Component::SafePointer<SeqArpPage> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, k] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f != juce::File()) safe->writeMidi (f.withFileExtension ("mid"), k);
                          });
}

void SeqArpPage::writeMidi (const juce::File& f, int k)
{
    // Convention : C4 (60) = note tenue la plus grave (Arp, +1 demi-ton par index) ou touche jouee (Seq)
    const auto p = proc.getPattern (layer, k);
    const int tpq = 960;
    const double stepTicks = double (tpq) * stepBeats (k);

    juce::MidiMessageSequence seq;
    for (int i = 0; i < p.n; ++i)
    {
        const auto& n = p.notes[i];
        const int note = juce::jlimit (0, 127, 60 + n.row);
        auto on = juce::MidiMessage::noteOn (1, note, (juce::uint8) n.vel);
        on.setTimeStamp (double (n.start) * stepTicks);
        auto off = juce::MidiMessage::noteOff (1, note);
        off.setTimeStamp (double (n.start + n.len) * stepTicks);
        seq.addEvent (on);
        seq.addEvent (off);
    }
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (seq);
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (os.openedOk()) mf.writeTo (os);
}

void SeqArpPage::importMidi()
{
    const int k = editing;
    chooser = std::make_unique<juce::FileChooser> ("Import MIDI into pattern",
                  juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid;*.midi");
    juce::Component::SafePointer<SeqArpPage> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, k] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile()) safe->readMidi (f, k);
                          });
}

void SeqArpPage::readMidi (const juce::File& f, int k)
{
    juce::FileInputStream is (f);
    if (! is.openedOk()) return;
    juce::MidiFile mf;
    if (! mf.readFrom (is)) return;
    const int tpq = mf.getTimeFormat();
    if (tpq <= 0) return;                       // timecode SMPTE non gere

    struct N { double on, off; int note, vel; };
    std::vector<N> notes;
    const double stepTicks = double (tpq) * stepBeats (k);
    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence s (*mf.getTrack (t));
        s.updateMatchedPairs();
        for (int i = 0; i < s.getNumEvents(); ++i)
        {
            const auto* ev = s.getEventPointer (i);
            if (! ev->message.isNoteOn()) continue;
            const double on = ev->message.getTimeStamp();
            const double off = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() : on + stepTicks;
            notes.push_back ({ on, off, ev->message.getNoteNumber(), (int) ev->message.getVelocity() });
        }
    }
    if (notes.empty()) return;

    double first = notes.front().on;
    int lo = 127, hi = 0;
    for (const auto& n : notes) { first = std::min (first, n.on); lo = std::min (lo, n.note); hi = std::max (hi, n.note); }
    const double bar = double (tpq) * 4.0;
    const double shift = std::floor (first / bar) * bar;      // cale sur la mesure du premier evenement

    int root = 60;
    if (k == 0) root = lo;                                       // Arp : la plus grave = Note 1
    else if (lo - root < -24 || hi - root > 24) root = (hi - lo <= 48) ? lo + (hi - lo) / 2 : lo;

    Pattern p;
    int lastEnd = 1;
    for (const auto& n : notes)
    {
        const int start = juce::roundToInt ((n.on - shift) / stepTicks);
        const int len = std::max (1, juce::roundToInt ((n.off - n.on) / stepTicks));
        if (start < 0 || start >= kMaxPatSteps) continue;
        const int row = (k == 0) ? juce::jlimit (0, 31, n.note - root) : juce::jlimit (-24, 24, n.note - root);
        if (p.add (start, len, row, n.vel)) lastEnd = std::max (lastEnd, start + len);
    }
    proc.setPattern (layer, k, p);
    setParam (proc.apvts, lpId (layer, k == 0 ? LP_ArpLen : LP_SeqLen), (float) juce::jlimit (1, kMaxPatSteps, lastEnd));
    roll (k).refresh();
}

void SeqArpPage::paint (juce::Graphics&) {}

void SeqArpPage::resized()
{
    // meme organisation que la page LAYER : sections, motif en bas jusqu'au bord droit
    auto a = getLocalBounds();
    auto panels = a.removeFromTop (std::min (200, a.getHeight() / 3));
    arpPanel.setBounds (panels.removeFromLeft (panels.getWidth() / 2).reduced (4));
    seqPanel.setBounds (panels.reduced (4));
    arpOn.setBounds (arpPanel.getRight() - 96, arpPanel.getY() + 6, 84, 22);
    seqOn.setBounds (seqPanel.getRight() - 96, seqPanel.getY() + 6, 84, 22);

    patternBox.setBounds (a.reduced (4));
    a = patternBox.getBounds().reduced (8).withTrimmedTop (24);
    auto bar = a.removeFromTop (30);
    for (auto& t : editTab) t.setBounds (bar.removeFromLeft (130).reduced (2, 2));
    exportBtn.setBounds (bar.removeFromRight (120).reduced (2, 2));
    importBtn.setBounds (bar.removeFromRight (120).reduced (2, 2));
    clearBtn.setBounds (bar.removeFromRight (70).reduced (2, 2));
    bar.removeFromRight (10);
    genBtn.setBounds (bar.removeFromRight (90).reduced (2, 2));
    genSpan.setBounds (bar.removeFromRight (90).reduced (2, 2));
    genType.setBounds (bar.removeFromRight (150).reduced (2, 2));
    genLabel.setBounds (bar.removeFromRight (80));

    a.removeFromTop (4);
    arpRoll.setBounds (a);
    seqRoll.setBounds (a);
}

// ============================================================================
// Chaines FX
// ============================================================================
using namespace aura::fx;

static juce::Colour fxCategoryColour (const juce::String& cat)
{
    if (cat == "Filter")     return juce::Colour (0xff4fc3f7);
    if (cat == "EQ")         return juce::Colour (0xff81c784);
    if (cat == "Dynamics")   return juce::Colour (0xffffb74d);
    if (cat == "Distortion") return juce::Colour (0xffe57373);
    if (cat == "Modulation") return juce::Colour (0xffba68c8);
    if (cat == "Delay")      return juce::Colour (0xff4db6ac);
    if (cat == "Reverb")     return juce::Colour (0xff7986cb);
    return juce::Colour (0xff90a4ae);
}

static const char* const kFxCategories[] = { "Filter", "EQ", "Dynamics", "Distortion", "Modulation", "Delay", "Reverb", "Utility" };

// ---------------- EqView ----------------
EqView::EqView (APVTS_& s, int chain, int slot)
{
    for (int i = 0; i < kFxParams; ++i) p[i] = s.getRawParameterValue (fxParamId (chain, slot, i));
    startTimerHz (15);
}

void EqView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawDisplayBg (g, b);
    const auto& def = fxDef (FxEq6);
    float real[kFxParams] {};
    for (int i = 0; i < def.numParams; ++i) real[i] = fxToReal (def.p[i], p[i]->load());
    Biquad bands[6];
    bool act[6];
    const float fs = 48000.f;
    eqDesign (bands, real, fs, act);
    const float outG = dbToGain (real[16]);

    auto xOf = [&] (float f) { return b.getX() + std::log (f / 20.f) / std::log (1000.f) * b.getWidth(); };
    auto yOf = [&] (float db) { return b.getCentreY() - juce::jlimit (-24.f, 24.f, db) / 24.f * b.getHeight() * 0.45f; };

    g.setColour (ui::outline);
    g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());

    juce::Path path;
    const int steps = std::max (2, (int) b.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float f = 20.f * std::pow (1000.f, float (i) / float (steps));
        float mag = outG;
        for (int k = 0; k < 6; ++k) if (act[k]) mag *= bands[k].magnitude (f, fs);
        const float y = yOf (20.f * std::log10 (std::max (mag, 1e-6f)));
        if (i == 0) path.startNewSubPath (xOf (f), y); else path.lineTo (xOf (f), y);
    }
    g.setColour (fxCategoryColour ("EQ"));
    g.strokePath (path, juce::PathStrokeType (2.f));

    const int fi[6] = { 0, 2, 5, 8, 11, 14 };
    const int gi[6] = { 1, 3, 6, 9, 12, 15 };
    g.setFont (juce::FontOptions (10.f));
    for (int k = 0; k < 6; ++k)
    {
        const float x = xOf (real[fi[k]]), y = yOf (real[gi[k]]);
        g.setColour (act[k] ? juce::Colours::white : ui::dim);
        g.fillEllipse (x - 4.f, y - 4.f, 8.f, 8.f);
        g.drawText (juce::String (k + 1), juce::Rectangle<float> (x - 8.f, y - 18.f, 16.f, 12.f), juce::Justification::centred);
    }
}

// ---------------- FxSlotPanel ----------------
static constexpr int kFxKnobW = 70, kFxKnobH = 70, kFxHeaderH = 28;

FxSlotPanel::FxSlotPanel (AuraProcessor& p, int c, int s) : proc (p), chain (c), slot (s), type (p.fxType (c, s))
{
    const auto& def = fxDef (type);
    addAndMakeVisible (power);
    power.setTooltip ("Effect on / off");
    powerA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, fxOnId (c, s), power);

    up.setTooltip ("Move up");
    down.setTooltip ("Move down");
    remove.setTooltip ("Remove effect");
    auto notify = [this]
    {
        juce::Component::SafePointer<FxSlotPanel> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr && safe->onStructureChanged) safe->onStructureChanged(); });
    };
    up.onClick     = [this, notify] { proc.moveFx (chain, slot, -1); notify(); };
    down.onClick   = [this, notify] { proc.moveFx (chain, slot, 1); notify(); };
    remove.onClick = [this, notify] { proc.removeFx (chain, slot); notify(); };
    for (auto* b : { &up, &down, &remove }) addAndMakeVisible (*b);

    for (int i = 0; i < def.numParams; ++i)
    {
        const FxParamSpec spec = def.p[i];
        auto k = std::make_unique<Knob>();
        styleKnob (k->s);
        k->s.setColour (juce::Slider::rotarySliderFillColourId, fxCategoryColour (def.category));
        k->l.setText (spec.name, juce::dontSendNotification);
        k->l.setJustificationType (juce::Justification::centred);
        k->l.setFont (juce::FontOptions (11.f));
        k->l.setColour (juce::Label::textColourId, ui::dim);
        addAndMakeVisible (k->s);
        addAndMakeVisible (k->l);
        k->a = std::make_unique<APVTS_::SliderAttachment> (p.apvts, fxParamId (c, s, i), k->s);
        k->s.textFromValueFunction = [spec] (double v) { return fxFormat (spec, fxToReal (spec, (float) v)); };
        k->s.valueFromTextFunction = [spec] (const juce::String& t)
        {
            float v = t.retainCharacters ("0123456789.-").getFloatValue();
            if (spec.unit == Unit::Pct) v /= 100.f;
            if (spec.unit == Unit::Hz && t.containsIgnoreCase ("k")) v *= 1000.f;
            return (double) fxToNorm (spec, v);
        };
        k->s.updateText();
        knobs.push_back (std::move (k));
    }
    if (type == FxEq6)
    {
        eq = std::make_unique<EqView> (p.apvts, c, s);
        addAndMakeVisible (*eq);
    }
}

int FxSlotPanel::preferredHeight (int width) const
{
    const int cols = std::max (1, (width - 16) / kFxKnobW);
    const int rows = ((int) knobs.size() + cols - 1) / cols;
    return kFxHeaderH + (eq != nullptr ? 110 : 0) + rows * kFxKnobH + 10;
}

void FxSlotPanel::paint (juce::Graphics& g)
{
    const auto& def = fxDef (type);
    const auto col = fxCategoryColour (def.category);
    const auto b = getLocalBounds().toFloat();
    g.setColour (ui::panel2);
    g.fillRoundedRectangle (b, 5.f);
    g.setColour (col);
    g.fillRoundedRectangle (b.withWidth (4.f), 2.f);
    const bool on = power.getToggleState();
    g.setColour (on ? ui::text : ui::dim);
    g.setFont (juce::FontOptions (14.f, juce::Font::bold));
    g.drawText (def.name, 36, 0, getWidth() - 150, kFxHeaderH, juce::Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText (def.category, 36, 0, getWidth() - 130, kFxHeaderH, juce::Justification::centredRight);
    if (! on)
    {
        g.setColour (ui::bg.withAlpha (0.45f));
        g.fillRoundedRectangle (b.withTrimmedTop ((float) kFxHeaderH), 5.f);
    }
}

void FxSlotPanel::resized()
{
    power.setBounds (8, 3, 24, 22);
    auto hdr = juce::Rectangle<int> (0, 0, getWidth(), kFxHeaderH).reduced (4, 3);
    remove.setBounds (hdr.removeFromRight (26));
    hdr.removeFromRight (2);
    down.setBounds (hdr.removeFromRight (26));
    up.setBounds (hdr.removeFromRight (26));

    auto a = getLocalBounds().withTrimmedTop (kFxHeaderH).reduced (8, 2);
    if (eq != nullptr)
    {
        eq->setBounds (a.removeFromTop (104));
        a.removeFromTop (6);
    }
    const int cols = std::max (1, (getWidth() - 16) / kFxKnobW);
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int cx = a.getX() + int (i % (size_t) cols) * kFxKnobW;
        const int cy = a.getY() + int (i / (size_t) cols) * kFxKnobH;
        auto cell = juce::Rectangle<int> (cx, cy, kFxKnobW, kFxKnobH).reduced (2);
        knobs[i]->l.setBounds (cell.removeFromTop (14));
        knobs[i]->s.setBounds (cell);
    }
}

// ---------------- FxChainEditor ----------------
FxChainEditor::FxChainEditor (AuraProcessor& p, int c) : proc (p), chain (c)
{
    title.setFont (juce::FontOptions (15.f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, ui::text);
    addAndMakeVisible (title);

    groupGain.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    groupGain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    groupGain.setPopupDisplayEnabled (true, true, nullptr);
    addChildComponent (groupGain);
    gainLabel.setText ("Level", juce::dontSendNotification);
    gainLabel.setFont (juce::FontOptions (11.f));
    gainLabel.setColour (juce::Label::textColourId, ui::dim);
    gainLabel.setJustificationType (juce::Justification::centredRight);
    addChildComponent (gainLabel);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    addBtn.setColour (juce::TextButton::buttonColourId, ui::accent.withAlpha (0.25f));
    addBtn.onClick = [this] { showAddMenu(); };
    content.addAndMakeVisible (addBtn);
    emptyHint.setText ("No effect in this chain yet. Click + to add one.", juce::dontSendNotification);
    emptyHint.setColour (juce::Label::textColourId, ui::dim);
    emptyHint.setJustificationType (juce::Justification::centred);
    content.addChildComponent (emptyHint);

    setChain (c);
    startTimerHz (5);
}

void FxChainEditor::setChain (int c)
{
    chain = juce::jlimit (0, kFxChains - 1, c);
    title.setText (fxChainName (chain) + (chain == kMasterChain ? "  (before output)" : "  (insert chain)"),
                   juce::dontSendNotification);
    groupGainA.reset();
    const bool group = chain < kFxGroups;
    if (group) groupGainA = std::make_unique<APVTS_::SliderAttachment> (proc.apvts, fxGainId (chain), groupGain);
    groupGain.setVisible (group);
    gainLabel.setVisible (group);
    rebuild();
}

int FxChainEditor::computeSignature() const
{
    int h = chain + 1;
    for (int s = 0; s < kFxSlots; ++s) h = h * 31 + proc.fxType (chain, s) + 1;
    return h;
}

void FxChainEditor::timerCallback()
{
    if (computeSignature() != signature) rebuild();
}

void FxChainEditor::rebuild()
{
    panels.clear();
    for (int s = 0; s < kFxSlots; ++s)
    {
        if (proc.fxType (chain, s) == FxNone) continue;
        auto pnl = std::make_unique<FxSlotPanel> (proc, chain, s);
        pnl->onStructureChanged = [this] { rebuild(); };
        content.addAndMakeVisible (*pnl);
        panels.push_back (std::move (pnl));
    }
    signature = computeSignature();
    addBtn.setEnabled ((int) panels.size() < kFxSlots);
    addBtn.setButtonText ((int) panels.size() < kFxSlots ? "+  Add effect" : "Chain full (6 effects)");
    layoutContent();
}

void FxChainEditor::layoutContent()
{
    const int w = std::max (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
    int y = 0;
    for (auto& pnl : panels)
    {
        const int h = pnl->preferredHeight (w);
        pnl->setBounds (0, y, w, h);
        y += h + 6;
    }
    emptyHint.setVisible (panels.empty());
    if (panels.empty()) { emptyHint.setBounds (0, y, w, 40); y += 44; }
    addBtn.setBounds (0, y, w, 30);
    content.setSize (w, y + 36);
}

void FxChainEditor::showAddMenu()
{
    juce::PopupMenu menu;
    for (const char* cat : kFxCategories)
    {
        juce::PopupMenu sub;
        for (int t = 1; t < kNumFxTypes; ++t)
            if (juce::String (fxDef (t).category) == cat) sub.addItem (t, fxDef (t).name);
        menu.addSubMenu (cat, sub);
    }
    juce::Component::SafePointer<FxChainEditor> safe (this);
    const int ch = chain;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addBtn),
                        [safe, ch] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            safe->proc.addFx (ch, result);
                            safe->rebuild();
                            safe->viewport.setViewPosition (0, safe->content.getHeight());
                        });
}

void FxChainEditor::paint (juce::Graphics& g)
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.f);
    g.setColour (chain == kMasterChain ? ui::accent : FxNode::chainColour (chain));
    g.fillRoundedRectangle (juce::Rectangle<float> (10.f, 13.f, 3.f, 14.f), 1.5f);
}

void FxChainEditor::resized()
{
    auto a = getLocalBounds().reduced (8);
    auto hdr = a.removeFromTop (40);
    hdr.removeFromLeft (10);
    groupGain.setBounds (hdr.removeFromRight (38).withSizeKeepingCentre (36, 36));
    gainLabel.setBounds (hdr.removeFromRight (40));
    title.setBounds (hdr);
    a.removeFromTop (4);
    viewport.setBounds (a);
    layoutContent();
}

// ============================================================================
// Page ROUTING
// ============================================================================
static void setupMiniKnob (juce::Slider& s, juce::Colour c)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId, c);
    s.setPopupDisplayEnabled (true, true, nullptr);
}

static void paintNodeBox (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c, bool selected)
{
    g.setColour (ui::panel2);
    g.fillRoundedRectangle (b, 5.f);
    g.setColour (selected ? juce::Colours::white : ui::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 5.f, selected ? 2.f : 1.f);
    g.setColour (c);
    g.fillRoundedRectangle (b.withHeight (3.f).reduced (4.f, 0.f), 1.5f);
}

LayerNode::LayerNode (AuraProcessor& p, int layerIndex) : proc (p), layer (layerIndex)
{
    const auto c = ui::layerColour (layer);
    addAndMakeVisible (power);
    powerA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, lpId (layer, LP_Enable), power);
    setupMiniKnob (vol, c);
    setupMiniKnob (pan, c);
    addAndMakeVisible (vol);
    addAndMakeVisible (pan);
    volA = std::make_unique<APVTS_::SliderAttachment> (p.apvts, lpId (layer, LP_Level), vol);
    panA = std::make_unique<APVTS_::SliderAttachment> (p.apvts, lpId (layer, LP_Pan), pan);
    enP  = p.apvts.getRawParameterValue (lpId (layer, LP_Enable));
    arpP = p.apvts.getRawParameterValue (lpId (layer, LP_Arp));
    seqP = p.apvts.getRawParameterValue (lpId (layer, LP_SeqOn));
    startTimerHz (5);
}

void LayerNode::resized()
{
    power.setBounds (4, 6, 24, 22);
    const int kw = 38, y = getHeight() - kw - 16;
    vol.setBounds (getWidth() / 2 - kw - 6, y, kw, kw);
    pan.setBounds (getWidth() / 2 + 6, y, kw, kw);
}

void LayerNode::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = ui::layerColour (layer);
    paintNodeBox (g, b, c, selected);

    g.setColour (ui::text);
    g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText (juce::String (layer + 1) + " - " + proc.getLayerName (layer), 28, 6, getWidth() - 32, 22,
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (11.f));
    g.setColour (c);
    g.drawText (ui::layerTypeText (proc.apvts, layer), 8, 30, 70, 16, juce::Justification::centredLeft);

    auto badge = [&] (const char* t, bool on, int x)
    {
        const juce::Rectangle<float> r ((float) x, 30.f, 30.f, 15.f);
        g.setColour (on ? c.withAlpha (0.85f) : ui::outline);
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (on ? ui::bg : ui::dim);
        g.drawText (t, r, juce::Justification::centred);
    };
    badge ("ARP", arpP->load() > 0.5f, getWidth() - 72);
    badge ("SEQ", seqP->load() > 0.5f, getWidth() - 38);

    g.setColour (ui::dim);
    g.drawText ("Vol", vol.getBounds().translated (0, vol.getHeight() - 2).withHeight (14), juce::Justification::centred);
    g.drawText ("Pan", pan.getBounds().translated (0, pan.getHeight() - 2).withHeight (14), juce::Justification::centred);

    if (enP->load() < 0.5f)
    {
        g.setColour (ui::bg.withAlpha (0.55f));
        g.fillRoundedRectangle (b.withTrimmedTop (28.f), 5.f);
    }
}

juce::Colour FxNode::chainColour (int c)
{
    static const juce::uint32 cols[kFxGroups] = { 0xff9575cd, 0xff4db6ac, 0xff7986cb, 0xffffb74d };
    return c >= kFxGroups ? ui::accent : juce::Colour (cols[juce::jlimit (0, kFxGroups - 1, c)]);
}

FxNode::FxNode (AuraProcessor& p, int c) : proc (p), chain (c)
{
    startTimerHz (4);
}

void FxNode::paint (juce::Graphics& g)
{
    paintNodeBox (g, getLocalBounds().toFloat(), chainColour (chain), selected);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText (fxChainName (chain), 10, 6, getWidth() - 20, 20, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (11.f));
    int y = 30, shown = 0;
    const int maxRows = (getHeight() - 36) / 19;
    for (int s = 0; s < kFxSlots; ++s)
    {
        const int t = proc.fxType (chain, s);
        if (t == FxNone) continue;
        if (shown >= maxRows) break;
        const bool on = proc.apvts.getRawParameterValue (fxOnId (chain, s))->load() > 0.5f;
        const juce::Rectangle<float> r (8.f, (float) y, (float) getWidth() - 16.f, 17.f);
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (on ? fxCategoryColour (fxDef (t).category) : ui::outline);
        g.fillRect (r.withWidth (3.f));
        g.setColour (on ? ui::text : ui::dim);
        g.drawText (fxDef (t).name, r.withTrimmedLeft (9.f), juce::Justification::centredLeft);
        y += 19;
        ++shown;
    }
    if (shown == 0)
    {
        g.setColour (ui::dim);
        g.drawText ("empty - click, then +", juce::Rectangle<int> (8, 30, getWidth() - 16, 18), juce::Justification::centredLeft);
    }
}

RoutingGraph::RoutingGraph (AuraProcessor& p) : proc (p)
{
    for (int l = 0; l < kLayers; ++l)
    {
        auto n = std::make_unique<LayerNode> (p, l);
        n->onSelect = [this, l] { if (onSelect) onSelect (l); };
        n->onEdit = [this, l] { if (onEditLayer) onEditLayer (l); };
        addAndMakeVisible (*n);
        layerNodes.push_back (std::move (n));
    }
    for (int c = 0; c < kFxChains; ++c)
    {
        auto n = std::make_unique<FxNode> (p, c);
        n->onSelect = [this, c] { if (onSelect) onSelect (chainIndex (c)); };
        addAndMakeVisible (*n);
        fxNodes.push_back (std::move (n));
    }
    startTimerHz (10);
}

void RoutingGraph::setSelected (int idx)
{
    for (int l = 0; l < kLayers; ++l) { layerNodes[(size_t) l]->selected = (idx == l); layerNodes[(size_t) l]->repaint(); }
    for (int c = 0; c < kFxChains; ++c) { fxNodes[(size_t) c]->selected = (idx == chainIndex (c)); fxNodes[(size_t) c]->repaint(); }
}

void RoutingGraph::resized()
{
    const int w = getWidth(), h = getHeight();
    const int nodeW = std::min (180, (w - 40) / kLayers - 16), nodeH = 108;
    const int gap = (w - kLayers * nodeW) / (kLayers + 1);
    for (int l = 0; l < kLayers; ++l)
        layerNodes[(size_t) l]->setBounds (gap + l * (nodeW + gap), 16, nodeW, nodeH);

    const int gW = std::min (170, (w - 40) / kFxGroups - 16), gH = 112;
    const int gGap = (w - kFxGroups * gW) / (kFxGroups + 1);
    const int gY = h * 40 / 100 - gH / 2;
    for (int c = 0; c < kFxGroups; ++c)
        fxNodes[(size_t) c]->setBounds (gGap + c * (gW + gGap), gY, gW, gH);

    const int mW = 230, mH = 140;
    fxNodes[(size_t) kMasterChain]->setBounds (w / 2 - mW / 2, h * 72 / 100 - mH / 2, mW, mH);
    outBox = juce::Rectangle<int> (w / 2 - 40, fxNodes[(size_t) kMasterChain]->getBottom() + 22, 80, 26);
}

static void drawWire (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour c, float amount, bool faint = false)
{
    juce::Path p;
    p.startNewSubPath (a);
    const float dy = std::max (30.f, (b.y - a.y) * 0.5f);
    p.cubicTo (a.x, a.y + dy, b.x, b.y - dy, b.x, b.y);
    if (faint)
    {
        const float dash[2] = { 4.f, 4.f };
        juce::Path d;
        juce::PathStrokeType (1.f).createDashedStroke (d, p, dash, 2);
        g.setColour (c.withAlpha (0.25f));
        g.fillPath (d);
        return;
    }
    g.setColour (c.withAlpha (0.3f + 0.6f * std::min (1.f, amount)));
    g.strokePath (p, juce::PathStrokeType (1.f + 3.f * std::min (1.f, amount)));
}

void RoutingGraph::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e1014));
    g.setColour (ui::outline.withAlpha (0.5f));
    for (int x = 12; x < getWidth(); x += 24)
        for (int y = 12; y < getHeight(); y += 24)
            g.fillRect ((float) x, (float) y, 1.5f, 1.5f);

    auto raw = [this] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    auto topOf = [] (const juce::Component& c) { return juce::Point<float> ((float) c.getBounds().getCentreX(), (float) c.getY()); };
    auto botOf = [] (const juce::Component& c) { return juce::Point<float> ((float) c.getBounds().getCentreX(), (float) c.getBottom()); };
    const auto& masterNode = *fxNodes[(size_t) kMasterChain];

    bool groupUsed[kFxGroups] {};
    for (int l = 0; l < kLayers; ++l)
    {
        if (raw (lpId (l, LP_Enable)) < 0.5f) continue;
        const int out = juce::jlimit (0, kFxGroups, juce::roundToInt (raw (lpId (l, LP_Output))));
        const auto& dst = out == 0 ? masterNode : *fxNodes[(size_t) out - 1];
        if (out > 0) groupUsed[out - 1] = true;
        drawWire (g, botOf (*layerNodes[(size_t) l]), topOf (dst), ui::layerColour (l), raw (lpId (l, LP_Level)));
    }
    for (int c = 0; c < kFxGroups; ++c)
        drawWire (g, botOf (*fxNodes[(size_t) c]), topOf (masterNode), FxNode::chainColour (c),
                  raw (fxGainId (c)), ! groupUsed[c]);

    drawWire (g, botOf (masterNode), { (float) outBox.getCentreX(), (float) outBox.getY() }, ui::accent, raw ("master"));
    g.setColour (ui::panel2);
    g.fillRoundedRectangle (outBox.toFloat(), 4.f);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (12.f, juce::Font::bold));
    g.drawText ("OUT", outBox, juce::Justification::centred);

    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText ("Layer > its Output (Master or FX 1-4) > Master FX > OUT.  Click a node to edit it, double-click a layer to open it.",
                8, getHeight() - 20, getWidth() - 16, 16, juce::Justification::centredLeft);
}

RoutingPage::RoutingPage (AuraProcessor& p) : proc (p), graph (p), chainEditor (p, 0)
{
    auto& s = p.apvts;
    for (int l = 0; l < kLayers; ++l)
    {
        auto pp = std::make_unique<ParamPanel> (s, p.getLayerName (l), ui::layerColour (l), 3);
        pp->addCombo (lpId (l, LP_Output), "Output", { "Master", "FX 1", "FX 2", "FX 3", "FX 4" });
        pp->addToggle (lpId (l, LP_Enable), "Enabled");
        pp->addToggle (lpId (l, LP_Arp), "Arpeggiator");
        pp->addToggle (lpId (l, LP_SeqOn), "Sequencer");
        pp->addKnob (lpId (l, LP_Level), "Level");
        pp->addKnob (lpId (l, LP_Pan), "Pan");
        pp->addKnob (lpId (l, LP_Transpose), "Transpose");
        pp->addKnob (lpId (l, LP_KeyLo), "Key Lo");
        pp->addKnob (lpId (l, LP_KeyHi), "Key Hi");
        pp->addKnob (lpId (l, LP_VelLo), "Vel Lo");
        pp->addKnob (lpId (l, LP_VelHi), "Vel Hi");
        pp->setAction ("Edit layer " + juce::String (l + 1), [this, l] { if (onEditLayer) onEditLayer (l); });
        addChildComponent (*pp);
        layerPanels.push_back (std::move (pp));
    }
    addChildComponent (chainEditor);
    addAndMakeVisible (graph);
    graph.onSelect = [this] (int i) { select (i); };
    graph.onEditLayer = [this] (int l) { if (onEditLayer) onEditLayer (l); };
    select (0);
    startTimerHz (2);
}

void RoutingPage::timerCallback()
{
    for (int l = 0; l < kLayers; ++l) layerPanels[(size_t) l]->setTitle (proc.getLayerName (l));
}

void RoutingPage::select (int idx)
{
    selected = juce::jlimit (0, kLayers + kFxChains - 1, idx);
    for (int l = 0; l < kLayers; ++l) layerPanels[(size_t) l]->setVisible (selected == l);
    if (selected >= kLayers)
    {
        if (chainEditor.getChain() != selected - kLayers) chainEditor.setChain (selected - kLayers);
        chainEditor.setVisible (true);
    }
    else
    {
        chainEditor.setVisible (false);
    }
    graph.setSelected (selected);
}

void RoutingPage::resized()
{
    auto a = getLocalBounds();
    auto side = a.removeFromRight (std::min (400, a.getWidth() * 36 / 100));
    for (auto& pp : layerPanels) pp->setBounds (side.reduced (4));
    chainEditor.setBounds (side.reduced (4));
    graph.setBounds (a.reduced (4));
}

// ============================================================================
// Page FX
// ============================================================================
FxPage::FxPage (AuraProcessor& p) : editor (p, 0), output (p.apvts, "Output", ui::accent, 2)
{
    for (int c = 0; c < kFxChains; ++c)
    {
        chainBtn[c].setButtonText (fxChainName (c));
        styleToggleButton (chainBtn[c], FxNode::chainColour (c));
        chainBtn[c].onClick = [this, c] { showChain (c); };
        addAndMakeVisible (chainBtn[c]);
    }
    addAndMakeVisible (editor);
    output.addToggle ("limOn", "Limiter (look-ahead)");
    output.addKnob ("limCeil", "Ceiling dB");
    output.addKnob ("limRel", "Release ms");
    output.addKnob ("master", "Master");
    addAndMakeVisible (output);
    showChain (0);
}

void FxPage::showChain (int c)
{
    for (int i = 0; i < kFxChains; ++i) chainBtn[i].setToggleState (i == c, juce::dontSendNotification);
    editor.setChain (c);
}

void FxPage::resized()
{
    auto a = getLocalBounds();
    auto bar = a.removeFromTop (34);
    for (auto& b : chainBtn) b.setBounds (bar.removeFromLeft (120).reduced (3, 2));
    a.removeFromTop (6);
    output.setBounds (a.removeFromRight (230).withHeight (std::min (a.getHeight(), 260)).reduced (4, 0));
    a.removeFromRight (6);
    editor.setBounds (a);
}

// ============================================================================
// Page LIB : browser de presets
// ============================================================================
static void styleEditor (juce::TextEditor& e, const juce::String& hint)
{
    e.setTextToShowWhenEmpty (hint, ui::dim);
    e.setColour (juce::TextEditor::backgroundColourId, ui::panel2);
    e.setColour (juce::TextEditor::outlineColourId, ui::outline);
    e.setColour (juce::TextEditor::focusedOutlineColourId, ui::accent);
    e.setColour (juce::TextEditor::textColourId, ui::text);
}

BrowserPage::BrowserPage (AuraProcessor& p) : proc (p)
{
    juce::StringArray cats { "All", "Favorites" };
    cats.addArray (PresetLibrary::categories());
    for (const auto& c : cats)
    {
        auto b = std::make_unique<juce::TextButton> (c);
        styleToggleButton (*b, ui::accent);
        b->onClick = [this, c] { setCategory (c); };
        addAndMakeVisible (*b);
        catButtons.push_back (std::move (b));
    }

    styleEditor (search, "Search name, tags, author...");
    search.onTextChange = [this] { refilter(); };
    addAndMakeVisible (search);

    list.setModel (this);
    list.setRowHeight (30);
    list.setColour (juce::ListBox::backgroundColourId, ui::panel);
    list.setColour (juce::ListBox::outlineColourId, ui::outline);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    info.setJustificationType (juce::Justification::topLeft);
    info.setColour (juce::Label::textColourId, ui::text);
    addAndMakeVisible (info);
    status.setColour (juce::Label::textColourId, ui::dim);
    status.setFont (juce::FontOptions (12.f));
    addAndMakeVisible (status);

    loadBtn.onClick   = [this] { loadRow (list.getSelectedRow()); };
    favBtn.onClick    = [this] { toggleFavorite(); };
    deleteBtn.onClick = [this] { doDelete(); };
    rescanBtn.onClick = [this] { proc.library.rescanAsync(); status.setText ("Scanning...", juce::dontSendNotification); };
    folderBtn.onClick = [] { PresetLibrary::presetsDir().createDirectory(); PresetLibrary::presetsDir().revealToUser(); };
    for (auto* b : { &loadBtn, &favBtn, &deleteBtn, &rescanBtn, &folderBtn }) addAndMakeVisible (*b);

    saveTitle.setText ("Save current sound", juce::dontSendNotification);
    saveTitle.setFont (juce::FontOptions (14.f, juce::Font::bold));
    addAndMakeVisible (saveTitle);
    styleEditor (saveName, "Preset name");
    styleEditor (saveTags, "Tags (comma separated: warm, analog, wide...)");
    styleEditor (saveAuthor, "Author");
    saveCat.addItemList (PresetLibrary::categories(), 1);
    saveCat.setSelectedId (1, juce::dontSendNotification);
    copySamples.setToggleState (true, juce::dontSendNotification);
    saveBtn.setColour (juce::TextButton::buttonColourId, ui::accent.withAlpha (0.35f));
    saveBtn.onClick = [this] { doSave(); };
    for (auto* c : std::initializer_list<juce::Component*> { &saveName, &saveTags, &saveAuthor, &saveCat, &copySamples, &saveBtn })
        addAndMakeVisible (c);

    setCategory ("All");
    proc.library.addChangeListener (this);
    if (proc.library.hasScanned()) changeListenerCallback (nullptr);
    else { proc.library.rescanAsync(); status.setText ("Scanning presets...", juce::dontSendNotification); }
    startTimerHz (2);
}

BrowserPage::~BrowserPage()
{
    proc.library.removeChangeListener (this);
    list.setModel (nullptr);
}

void BrowserPage::changeListenerCallback (juce::ChangeBroadcaster*)
{
    all = proc.library.snapshot();
    status.setText (juce::String ((int) all.size()) + " preset(s) in " + PresetLibrary::presetsDir().getFullPathName(),
                    juce::dontSendNotification);
    refilter();
}

void BrowserPage::timerCallback()
{
    deleteBtn.setButtonText (confirmDelete ? "Confirm delete?" : "Delete");
}

void BrowserPage::setCategory (const juce::String& c)
{
    category = c;
    for (auto& b : catButtons) b->setToggleState (b->getButtonText() == c, juce::dontSendNotification);
    refilter();
}

void BrowserPage::refilter()
{
    const auto q = search.getText().trim().toLowerCase();
    const auto cur = proc.getPresetFile();
    shown.clear();
    for (const auto& p : all)
    {
        if (category == "Favorites" && ! p.favorite) continue;
        if (category != "All" && category != "Favorites" && p.category != category) continue;
        if (q.isNotEmpty() && ! (p.name.toLowerCase().contains (q) || p.tags.toLowerCase().contains (q)
                                 || p.author.toLowerCase().contains (q) || p.category.toLowerCase().contains (q)))
            continue;
        shown.push_back (p);
    }
    list.updateContent();
    list.deselectAllRows();
    for (size_t i = 0; i < shown.size(); ++i)
        if (shown[i].file == cur) { list.selectRow ((int) i); break; }
    list.repaint();
    updateInfo();
}

int BrowserPage::currentIndex() const
{
    const auto cur = proc.getPresetFile();
    for (size_t i = 0; i < shown.size(); ++i)
        if (shown[i].file == cur) return (int) i;
    return -1;
}

void BrowserPage::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& p = shown[(size_t) row];
    const bool loaded = p.file == proc.getPresetFile();
    if (selected) { g.setColour (ui::accent.withAlpha (0.18f)); g.fillRect (0, 0, w, h); }
    g.setColour (ui::outline.withAlpha (0.5f));
    g.drawHorizontalLine (h - 1, 0.f, (float) w);

    g.setColour (p.favorite ? juce::Colour (0xffffd54f) : ui::outline);
    g.setFont (juce::FontOptions (15.f));
    g.drawText (p.favorite ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x86")),
                6, 0, 20, h, juce::Justification::centred);

    g.setColour (loaded ? ui::accent : ui::text);
    g.setFont (juce::FontOptions (14.f, loaded ? juce::Font::bold : juce::Font::plain));
    g.drawText (p.name, 30, 0, w * 40 / 100, h, juce::Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (12.f));
    g.drawText (p.category, 30 + w * 40 / 100, 0, w * 18 / 100, h, juce::Justification::centredLeft);
    g.drawText (p.tags, 30 + w * 58 / 100, 0, w * 40 / 100 - 36, h, juce::Justification::centredLeft);
}

void BrowserPage::updateInfo()
{
    const int r = list.getSelectedRow();
    confirmDelete = false;
    if (r < 0 || r >= (int) shown.size())
    {
        info.setText ("Select a preset. Double-click to load.\n\nPresets are saved in:\n"
                      + PresetLibrary::presetsDir().getFullPathName()
                      + "\nCopied samples go to:\n" + PresetLibrary::samplesDir().getFullPathName(),
                      juce::dontSendNotification);
        return;
    }
    const auto& p = shown[(size_t) r];
    info.setText (p.name + "\n\nCategory: " + p.category
                  + "\nAuthor: " + (p.author.isNotEmpty() ? p.author : juce::String ("-"))
                  + "\nTags: " + (p.tags.isNotEmpty() ? p.tags : juce::String ("-"))
                  + "\nLayers: " + p.layers
                  + "\nModified: " + p.modified.toString (true, true, false),
                  juce::dontSendNotification);
    favBtn.setButtonText (p.favorite ? "Unfavorite" : "Favorite");
}

void BrowserPage::loadRow (int row)
{
    if (row < 0 || row >= (int) shown.size()) return;
    juce::String err;
    if (proc.loadPreset (shown[(size_t) row].file, err))
    {
        status.setText ("Loaded: " + shown[(size_t) row].name, juce::dontSendNotification);
        saveName.setText (shown[(size_t) row].name, false);
        saveTags.setText (shown[(size_t) row].tags, false);
        saveAuthor.setText (shown[(size_t) row].author, false);
        const int ci = PresetLibrary::categories().indexOf (shown[(size_t) row].category);
        if (ci >= 0) saveCat.setSelectedId (ci + 1, juce::dontSendNotification);
    }
    else
    {
        status.setText ("Error: " + err, juce::dontSendNotification);
    }
    list.repaint();
}

void BrowserPage::loadRelative (int delta)
{
    if (shown.empty()) return;
    int i = currentIndex();
    i = (i < 0) ? (delta > 0 ? 0 : (int) shown.size() - 1)
                : (i + delta + (int) shown.size()) % (int) shown.size();
    list.selectRow (i);
    loadRow (i);
}

void BrowserPage::focusSave()
{
    if (saveName.isEmpty()) saveName.setText (proc.getPresetName() == "Init" ? juce::String() : proc.getPresetName(), false);
    saveName.grabKeyboardFocus();
    saveName.selectAll();
}

void BrowserPage::doSave()
{
    const auto name = saveName.getText().trim();
    if (name.isEmpty()) { status.setText ("Enter a preset name first.", juce::dontSendNotification); return; }

    AuraProcessor::PresetMeta meta;
    meta.name = name;
    meta.category = saveCat.getText().isNotEmpty() ? saveCat.getText() : juce::String ("Other");
    meta.tags = saveTags.getText().trim();
    meta.author = saveAuthor.getText().trim();

    const auto file = PresetLibrary::presetsDir().getChildFile (meta.category.replace ("/", "-").trim())
                          .getChildFile (juce::File::createLegalFileName (name) + PresetLibrary::kExt);
    juce::String err;
    if (proc.savePreset (file, meta, copySamples.getToggleState(), err))
        status.setText ((file.existsAsFile() ? "Saved: " : "Saved ") + file.getFullPathName(), juce::dontSendNotification);
    else
        status.setText ("Error: " + err, juce::dontSendNotification);
}

void BrowserPage::doDelete()
{
    const int r = list.getSelectedRow();
    if (r < 0 || r >= (int) shown.size()) return;
    if (! confirmDelete) { confirmDelete = true; return; }      // deuxieme clic = confirmation
    confirmDelete = false;
    const auto f = shown[(size_t) r].file;
    if (f.moveToTrash() || f.deleteFile())
    {
        status.setText ("Deleted (moved to trash): " + shown[(size_t) r].name, juce::dontSendNotification);
        proc.library.rescanAsync();
    }
}

void BrowserPage::toggleFavorite()
{
    const int r = list.getSelectedRow();
    if (r < 0 || r >= (int) shown.size()) return;
    auto& p = shown[(size_t) r];
    if (! PresetLibrary::setFavorite (p.file, ! p.favorite)) return;
    p.favorite = ! p.favorite;
    for (auto& a : all) if (a.file == p.file) a.favorite = p.favorite;
    list.repaint();
    updateInfo();
}

void BrowserPage::paint (juce::Graphics& g)
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.f);
}

void BrowserPage::resized()
{
    auto a = getLocalBounds().reduced (8);
    status.setBounds (a.removeFromBottom (20));
    a.removeFromBottom (4);

    auto left = a.removeFromLeft (150);
    for (auto& b : catButtons)
    {
        b->setBounds (left.removeFromTop (28).reduced (0, 2));
    }
    a.removeFromLeft (8);

    auto right = a.removeFromRight (std::min (330, a.getWidth() / 3));
    a.removeFromRight (8);

    search.setBounds (a.removeFromTop (28));
    a.removeFromTop (6);
    list.setBounds (a);

    // panneau de droite : infos + actions + sauvegarde
    auto saveArea = right.removeFromBottom (230);
    auto btns = right.removeFromBottom (64);
    info.setBounds (right);
    auto b1 = btns.removeFromTop (30);
    const int bw = b1.getWidth() / 3;
    loadBtn.setBounds (b1.removeFromLeft (bw).reduced (2));
    favBtn.setBounds (b1.removeFromLeft (bw).reduced (2));
    deleteBtn.setBounds (b1.reduced (2));
    auto b2 = btns.removeFromTop (30);
    rescanBtn.setBounds (b2.removeFromLeft (b2.getWidth() / 2).reduced (2));
    folderBtn.setBounds (b2.reduced (2));

    saveArea.removeFromTop (8);
    saveTitle.setBounds (saveArea.removeFromTop (22));
    for (auto* c : std::initializer_list<juce::Component*> { &saveName, &saveTags, &saveAuthor, &saveCat })
    {
        c->setBounds (saveArea.removeFromTop (28));
        saveArea.removeFromTop (4);
    }
    copySamples.setBounds (saveArea.removeFromTop (26));
    saveBtn.setBounds (saveArea.removeFromTop (30).reduced (0, 2));
}

// ---------------- PresetBar ----------------
PresetBar::PresetBar (AuraProcessor& p) : proc (p)
{
    prev.onClick = [this] { if (onStep) onStep (-1); };
    next.onClick = [this] { if (onStep) onStep (1); };
    save.onClick = [this] { if (onSave) onSave(); };
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    save.setTooltip ("Save preset (LIB page)");
    for (auto* b : { &prev, &next, &save }) addAndMakeVisible (*b);
    startTimerHz (4);
    timerCallback();
}

void PresetBar::timerCallback()
{
    const auto n = proc.getPresetName();
    if (n != name) { name = n; repaint(); }
}

void PresetBar::resized()
{
    auto a = getLocalBounds();
    save.setBounds (a.removeFromRight (56).reduced (2, 4));
    next.setBounds (a.removeFromRight (30).reduced (2, 4));
    prev.setBounds (a.removeFromRight (30).reduced (2, 4));
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().withTrimmedRight (120.f);
    g.setColour (ui::panel2);
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (15.f));
    g.drawText (name, r.reduced (12.f, 0.f), juce::Justification::centredLeft);
}

// ============================================================================
// Colonne des couches
// ============================================================================
LayerRow::LayerRow (AuraProcessor& p, int layerIndex) : proc (p), layer (layerIndex)
{
    addAndMakeVisible (power);
    powerA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, lpId (layer, LP_Enable), power);

    name.setText (p.getLayerName (layer), juce::dontSendNotification);
    name.setFont (juce::FontOptions (14.f, juce::Font::bold));
    name.setEditable (false, true, false);                       // double-clic pour renommer
    name.setTooltip ("Double-click to rename");
    name.setColour (juce::Label::backgroundWhenEditingColourId, ui::panel2);
    name.setColour (juce::Label::textWhenEditingColourId, ui::text);
    name.onTextChange = [this] { proc.setLayerName (layer, name.getText()); };
    name.addMouseListener (this, false);                         // un simple clic selectionne la couche
    addAndMakeVisible (name);

    mute.setClickingTogglesState (true);
    solo.setClickingTogglesState (true);
    mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe57373));
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffffd54f));
    mute.setColour (juce::TextButton::textColourOnId, ui::bg);
    solo.setColour (juce::TextButton::textColourOnId, ui::bg);
    mute.setTooltip ("Mute");
    solo.setTooltip ("Solo");
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    muteA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, lpId (layer, LP_Mute), mute);
    soloA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, lpId (layer, LP_Solo), solo);

    level.setSliderStyle (juce::Slider::LinearBar);
    level.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    level.setColour (juce::Slider::trackColourId, ui::layerColour (layer).withAlpha (0.75f));
    level.setColour (juce::Slider::backgroundColourId, ui::bg);
    level.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    level.setPopupDisplayEnabled (true, true, nullptr);
    level.setTooltip ("Layer volume");
    addAndMakeVisible (level);
    levelA = std::make_unique<APVTS_::SliderAttachment> (p.apvts, lpId (layer, LP_Level), level);
    startTimerHz (4);
}

void LayerRow::timerCallback()
{
    if (! name.isBeingEdited())
    {
        const auto n = proc.getLayerName (layer);
        if (n != name.getText()) name.setText (n, juce::dontSendNotification);
    }
    const bool on = proc.apvts.getRawParameterValue (lpId (layer, LP_Enable))->load() > 0.5f;
    name.setColour (juce::Label::textColourId, on ? (selected ? ui::layerColour (layer) : ui::text) : ui::dim);
    if (isShowing()) repaint();
}

void LayerRow::resized()
{
    power.setBounds (8, 8, 24, 22);
    const int bs = 24;
    solo.setBounds (getWidth() - bs - 8, 8, bs, bs);
    mute.setBounds (getWidth() - 2 * bs - 12, 8, bs, bs);
    name.setBounds (34, 6, getWidth() - 34 - 2 * bs - 18, 24);
    level.setBounds (38, getHeight() - 22, getWidth() - 46, 12);
}

void LayerRow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.f, 2.f);
    const auto c = ui::layerColour (layer);
    g.setColour (selected ? ui::panel2 : ui::panel);
    g.fillRoundedRectangle (b, 5.f);
    g.setColour (c.withAlpha (selected ? 1.f : 0.5f));
    g.fillRoundedRectangle (b.removeFromLeft (4.f), 2.f);
    if (selected)
    {
        g.setColour (c.withAlpha (0.6f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f, 2.5f), 5.f, 1.f);
    }
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    const int out = juce::roundToInt (proc.apvts.getRawParameterValue (lpId (layer, LP_Output))->load());
    g.drawText (ui::layerTypeText (proc.apvts, layer) + "   >  " + (out == 0 ? juce::String ("Master") : "FX " + juce::String (out)),
                38, 30, getWidth() - 46, 16, juce::Justification::centredLeft);
}
// ============================================================================
// Editeur principal
// ============================================================================
AuraEditor::AuraEditor (AuraProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (p), browser (p), routing (p), fxPage (p)
{
    setLookAndFeel (&look);

    addAndMakeVisible (presetBar);
    addChildComponent (browser);
    presetBar.onStep = [this] (int d) { browser.loadRelative (d); };
    presetBar.onOpen = [this] { showPage (PageLib); };
    presetBar.onSave = [this] { showPage (PageLib); browser.focusSave(); };

    static const char* const names[kNumPages] = { "LIB", "LAYER", "ARP / SEQ", "ROUTING", "FX" };
    for (int i = 0; i < kNumPages; ++i)
    {
        nav[i].setButtonText (names[i]);
        nav[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (nav[i]);
    }

    output.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    output.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    output.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (output);
    outputA = std::make_unique<APVTS_::SliderAttachment> (p.apvts, "master", output);
    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    outputLabel.setColour (juce::Label::textColourId, ui::dim);
    outputLabel.setFont (juce::FontOptions (11.f));
    addAndMakeVisible (outputLabel);

    for (int l = 0; l < kLayers; ++l)
    {
        auto r = std::make_unique<LayerRow> (p, l);
        r->onSelect = [this, l] { selectLayer (l); if (page != PageSeq) showPage (PageLayer); };
        addAndMakeVisible (*r);
        rows.push_back (std::move (r));

        layerEditors.push_back (std::make_unique<LayerEditor> (p, l));
        addChildComponent (*layerEditors.back());
        seqPages.push_back (std::make_unique<SeqArpPage> (p, l));
        addChildComponent (*seqPages.back());
    }
    addChildComponent (routing);
    addChildComponent (fxPage);
    routing.onEditLayer = [this] (int l) { selectLayer (l); showPage (PageLayer); };

    selectLayer (0);
    showPage (PageLayer);

    setResizable (true, true);
    setResizeLimits (1100, 720, 2200, 1400);
    setSize (1280, 820);
}

AuraEditor::~AuraEditor()
{
    setLookAndFeel (nullptr);
}

void AuraEditor::selectLayer (int l)
{
    currentLayer = juce::jlimit (0, kLayers - 1, l);
    for (int i = 0; i < kLayers; ++i)
    {
        rows[(size_t) i]->selected = (i == currentLayer);
        rows[(size_t) i]->repaint();
    }
    showPage (page);
}

void AuraEditor::showPage (int p)
{
    page = juce::jlimit (0, kNumPages - 1, p);
    for (int i = 0; i < kNumPages; ++i) nav[i].setToggleState (i == page, juce::dontSendNotification);
    for (int i = 0; i < kLayers; ++i)
    {
        layerEditors[(size_t) i]->setVisible (page == PageLayer && i == currentLayer);
        seqPages[(size_t) i]->setVisible (page == PageSeq && i == currentLayer);
    }
    browser.setVisible (page == PageLib);
    routing.setVisible (page == PageRouting);
    fxPage.setVisible (page == PageFx);
    if (page == PageRouting) routing.select (currentLayer);
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
    layerEditors[(size_t) currentLayer]->loadFiles (list);
}

void AuraEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::panel);
    g.fillRect (0, 0, getWidth(), 52);
    g.setColour (ui::outline);
    g.drawHorizontalLine (52, 0.f, (float) getWidth());

    g.setColour (ui::accent);
    g.setFont (juce::FontOptions (26.f, juce::Font::bold));
    g.drawText ("AURA", 18, 8, 110, 36, juce::Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText ("v" JucePlugin_VersionString, 98, 22, 60, 16, juce::Justification::centredLeft);
}

void AuraEditor::resized()
{
    auto a = getLocalBounds();
    auto top = a.removeFromTop (52);
    auto out = top.removeFromRight (70);
    output.setBounds (out.removeFromTop (38).withSizeKeepingCentre (36, 36));
    outputLabel.setBounds (out);
    top.removeFromRight (12);
    for (int i = kNumPages - 1; i >= 0; --i)
    {
        nav[i].setBounds (top.removeFromRight (i == PageSeq ? 104 : 86).reduced (3, 10));
    }
    top.removeFromRight (12);
    top.removeFromLeft (165);
    presetBar.setBounds (top.withSizeKeepingCentre (std::min (460, top.getWidth()), 34));

    // colonne des couches a gauche, edition de la centrale jusqu'au bord droit
    auto column = a.removeFromLeft (230).reduced (6, 8);
    for (auto& r : rows)
    {
        r->setBounds (column.removeFromTop (74));
        column.removeFromTop (4);
    }

    const auto pageArea = a.reduced (4, 6);
    for (auto& e : layerEditors) e->setBounds (pageArea);
    for (auto& s : seqPages) s->setBounds (pageArea);
    browser.setBounds (pageArea);
    routing.setBounds (pageArea);
    fxPage.setBounds (pageArea);
}
