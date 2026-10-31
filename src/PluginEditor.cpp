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
void KnobSet::add (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 15);
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

// ---------------- StepSeqEditor ----------------
static constexpr float kSeqLabelW = 58.f;

StepSeqEditor::StepSeqEditor (AuraProcessor& p, int layerIndex) : proc (p), layer (layerIndex)
{
    for (int i = 0; i < kSeqSteps; ++i)
        for (int k = 0; k < 3; ++k)
            prm[i][k] = proc.apvts.getParameter (lpId (layer, lpStep (i, k)));
    lenParam = proc.apvts.getParameter (lpId (layer, LP_SeqLen));
    startTimerHz (30);
}

juce::Rectangle<float> StepSeqEditor::laneRect (int lane) const
{
    auto a = getLocalBounds().toFloat().reduced (4.f);
    a.removeFromLeft (kSeqLabelW);
    auto gate = a.removeFromTop (24.f);
    a.removeFromTop (6.f);
    const float h = (a.getHeight() - 6.f) * 0.5f;
    auto vel = a.removeFromTop (h);
    a.removeFromTop (6.f);
    if (lane == LaneGate) return gate;
    if (lane == LaneVel)  return vel;
    return a;
}

int StepSeqEditor::laneAt (juce::Point<float> p) const
{
    for (int l = 0; l < kNumLanes; ++l)
        if (laneRect (l).expanded (0.f, 3.f).contains (p)) return l;
    return -1;
}

int StepSeqEditor::stepAt (float x) const
{
    const auto r = laneRect (LaneGate);
    return juce::jlimit (0, kSeqSteps - 1, int ((x - r.getX()) / r.getWidth() * float (kSeqSteps)));
}

float StepSeqEditor::get (int step, int part) const
{
    auto* p = prm[step][part];
    return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.f;
}

void StepSeqEditor::set (int step, int part, float value)
{
    auto* p = prm[step][part];
    if (p == nullptr) return;
    const float nv = p->convertTo0to1 (value);
    if (std::abs (p->getValue() - nv) < 1e-6f) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (nv);
    p->endChangeGesture();
    repaint();
}

void StepSeqEditor::editAt (juce::Point<float> pt)
{
    const int s = stepAt (pt.x);
    const auto r = laneRect (dragLane);
    if (dragLane == LaneGate)
    {
        set (s, 0, paintOn ? 1.f : 0.f);
    }
    else if (dragLane == LaneVel)
    {
        set (s, 2, juce::jlimit (0.f, 1.f, (r.getBottom() - pt.y) / r.getHeight()));
    }
    else if (dragLane == LanePitch)
    {
        const float v = (r.getCentreY() - pt.y) / (r.getHeight() * 0.5f) * 24.f;
        set (s, 1, (float) juce::jlimit (-24, 24, juce::roundToInt (v)));
    }
}

void StepSeqEditor::mouseDown (const juce::MouseEvent& e)
{
    dragLane = laneAt (e.position);
    if (dragLane < 0) return;
    if (dragLane == LaneGate) paintOn = get (stepAt (e.position.x), 0) < 0.5f;
    editAt (e.position);
}

void StepSeqEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragLane >= 0) editAt (e.position);
}

void StepSeqEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position);
    const int s = stepAt (e.position.x);
    if (lane == LaneVel)   set (s, 2, 0.8f);
    if (lane == LanePitch) set (s, 1, 0.f);
}

void StepSeqEditor::timerCallback()
{
    if (! isShowing()) return;
    const int st = proc.getSeqStep (layer);
    if (st != lastStep || ++tick >= 6)   // pas courant + rafraichissement automation (~5 Hz)
    {
        lastStep = st;
        tick = 0;
        repaint();
    }
}

void StepSeqEditor::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0d0f13));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.f);

    const int len = lenParam != nullptr ? juce::roundToInt (lenParam->convertFrom0to1 (lenParam->getValue())) : kSeqSteps;
    const int cur = proc.getSeqStep (layer);
    const juce::Colour col = ui::layerColour (layer);

    static const char* const names[kNumLanes] = { "Gate", "Velocity", "Pitch" };
    g.setFont (juce::FontOptions (12.f));
    for (int l = 0; l < kNumLanes; ++l)
    {
        const auto r = laneRect (l);
        g.setColour (ui::dim);
        g.drawText (names[l], juce::Rectangle<float> (8.f, r.getY(), kSeqLabelW - 10.f, r.getHeight()),
                    juce::Justification::centredLeft);
        g.setColour (ui::panel);
        g.fillRect (r);
    }

    const auto gate = laneRect (LaneGate), vel = laneRect (LaneVel), pit = laneRect (LanePitch);
    const float w = gate.getWidth() / float (kSeqSteps);

    for (int i = 0; i < kSeqSteps; ++i)
    {
        const float x = gate.getX() + float (i) * w;
        const bool inLen = i < len;
        const bool on = get (i, 0) > 0.5f;
        const float alpha = inLen ? 1.f : 0.25f;

        if (i == cur)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (x, gate.getY(), w, pit.getBottom() - gate.getY());
        }
        if (i % 4 == 0)
        {
            g.setColour (ui::outline);
            g.drawVerticalLine ((int) x, gate.getY(), pit.getBottom());
        }

        auto cell = juce::Rectangle<float> (x, gate.getY(), w, gate.getHeight()).reduced (3.f);
        if (on) { g.setColour (col.withAlpha (alpha)); g.fillRoundedRectangle (cell, 3.f); }
        else    { g.setColour (ui::dim.withAlpha (alpha)); g.drawRoundedRectangle (cell, 3.f, 1.f); }
        if (i == cur && on) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (cell, 3.f, 2.f); }

        const float v = get (i, 2);
        g.setColour (col.withAlpha ((on ? 0.85f : 0.3f) * alpha));
        g.fillRect (juce::Rectangle<float> (x + 3.f, vel.getBottom() - v * vel.getHeight(), w - 6.f, v * vel.getHeight()));

        const int pt = juce::roundToInt (get (i, 1));
        const float half = pit.getHeight() * 0.5f;
        const float py = pit.getCentreY() - float (pt) / 24.f * half;
        g.setColour (juce::Colours::orange.withAlpha ((on ? 0.85f : 0.3f) * alpha));
        g.fillRect (juce::Rectangle<float> (x + 3.f, std::min (py, pit.getCentreY()), w - 6.f,
                                            std::max (2.f, std::abs (py - pit.getCentreY()))));
        if (pt != 0)
        {
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.drawText ((pt > 0 ? "+" : "") + juce::String (pt),
                        juce::Rectangle<float> (x, pit.getY() + 2.f, w, 14.f), juce::Justification::centred);
        }
    }

    g.setColour (ui::outline);
    g.drawHorizontalLine ((int) pit.getCentreY(), pit.getX(), pit.getRight());
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

MsegPane::MsegPane (AuraProcessor& p, int layer, int idx) : knobs (*this, p.apvts)
{
    addAndMakeVisible (editor);
    editor.setShape (p.getMseg (layer, idx));
    auto* proc = &p;
    editor.onChange = [proc, layer, idx] (const MsegShape& s) { proc->setMseg (layer, idx, s); };
    loop.init (*this, p.apvts, lpId (layer, idx == 0 ? LP_Mseg1Loop : LP_Mseg2Loop), "Loop");
    knobs.add (lpId (layer, idx == 0 ? LP_Mseg1Time : LP_Mseg2Time), "Time (s)");

    hint.setText ("Double-click: add / delete point. Drag diamond: curve.", juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, ui::dim);
    hint.setFont (juce::FontOptions (11.f));
    addAndMakeVisible (hint);
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

SeqPane::SeqPane (AuraProcessor& p, int layer) : knobs (*this, p.apvts), editor (p, layer)
{
    on.init (*this, p.apvts, lpId (layer, LP_SeqOn), "Seq on");
    rate.init (*this, p.apvts, lpId (layer, LP_SeqRate), "Rate", kRateItems);
    knobs.add (lpId (layer, LP_SeqLen),   "Length");
    knobs.add (lpId (layer, LP_SeqGate),  "Gate");
    knobs.add (lpId (layer, LP_SeqSwing), "Swing");
    addAndMakeVisible (editor);

    hint.setText ("Hold keys: active steps replay them, transposed by Pitch. Double-click resets.",
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, ui::dim);
    hint.setFont (juce::FontOptions (11.f));
    addAndMakeVisible (hint);
}

void SeqPane::resized()
{
    auto a = getLocalBounds().reduced (6);
    hint.setBounds (a.removeFromBottom (16));
    auto side = a.removeFromLeft (150);
    a.removeFromLeft (6);
    editor.setBounds (a);
    on.button.setBounds (side.removeFromTop (24));
    side.removeFromTop (4);
    auto r = side.removeFromTop (24);
    rate.place (r, 40, 100);
    side.removeFromTop (4);
    knobs.layout (side, 2);
}

ModSourcesSection::ModSourcesSection (AuraProcessor& p, int layer)
    : Section ("Modulation sources", ui::layerColour (layer)),
      env (p, layer), lfo1 (p, layer, 0), lfo2 (p, layer, 1),
      mseg1 (p, layer, 0), mseg2 (p, layer, 1), seq (p, layer)
{
    const auto bgc = ui::panel;
    tabs.addTab ("MOD ENV", bgc, &env, false);
    tabs.addTab ("LFO 1",   bgc, &lfo1, false);
    tabs.addTab ("LFO 2",   bgc, &lfo2, false);
    tabs.addTab ("MSEG 1",  bgc, &mseg1, false);
    tabs.addTab ("MSEG 2",  bgc, &mseg2, false);
    tabs.addTab ("STEP SEQ", bgc, &seq, false);
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

ZoneSection::ZoneSection (AuraProcessor& p, int layer)
    : Section ("Zone / Sends / Macros", ui::layerColour (layer)), knobs (*this, p.apvts)
{
    arp.init (*this, p.apvts, lpId (layer, LP_Arp), "Arp");
    knobs.add (lpId (layer, LP_KeyLo),     "Key Lo");
    knobs.add (lpId (layer, LP_KeyHi),     "Key Hi");
    knobs.add (lpId (layer, LP_VelLo),     "Vel Lo");
    knobs.add (lpId (layer, LP_VelHi),     "Vel Hi");
    knobs.add (lpId (layer, LP_Transpose), "Transpose");
    knobs.add (lpId (layer, LP_SendCho),   "> Chorus");
    knobs.add (lpId (layer, LP_SendDly),   "> Delay");
    knobs.add (lpId (layer, LP_SendRev),   "> Reverb");
    knobs.add (lpId (layer, LP_Macro1),    "Macro 1");
    knobs.add (lpId (layer, LP_Macro2),    "Macro 2");
    knobs.add (lpId (layer, LP_Macro3),    "Macro 3");
    knobs.add (lpId (layer, LP_Macro4),    "Macro 4");
}

void ZoneSection::resized()
{
    auto a = content();
    arp.button.setBounds (a.removeFromLeft (70).withSizeKeepingCentre (70, 26));
    knobs.layout (a, 12);
}

LayerEditor::LayerEditor (AuraProcessor& p, int layer)
    : osc (p, layer), filter (p, layer), amp (p, layer), mods (p, layer), matrix (p, layer), zone (p, layer)
{
    for (auto* c : std::initializer_list<juce::Component*> { &osc, &filter, &amp, &mods, &matrix, &zone })
        addAndMakeVisible (c);
}

void LayerEditor::resized()
{
    auto a = getLocalBounds();
    auto top = a.removeFromTop (a.getHeight() * 44 / 100);
    auto bottom = a.removeFromBottom (122);

    osc.setBounds (top.removeFromLeft (top.getWidth() * 50 / 100).reduced (4));
    filter.setBounds (top.removeFromLeft (top.getWidth() * 40 / 100).reduced (4));
    amp.setBounds (top.reduced (4));

    mods.setBounds (a.removeFromLeft (a.getWidth() * 58 / 100).reduced (4));
    matrix.setBounds (a.reduced (4));

    zone.setBounds (bottom.reduced (4));
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
    const int kw = 40, y = getHeight() - kw - 16;
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
    g.drawText (juce::String (layer + 1) + " - Layer " + juce::String (layer + 1),
                28, 6, getWidth() - 32, 22, juce::Justification::centredLeft);

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

juce::Colour FxBlockNode::rowColour (int i)
{
    static const juce::uint32 c[kFx] = { 0xff9575cd, 0xff4db6ac, 0xff7986cb };
    return juce::Colour (c[juce::jlimit (0, kFx - 1, i)]);
}

const char* FxBlockNode::rowName (int i)
{
    static const char* const n[kFx] = { "Chorus", "Delay", "Reverb" };
    return n[juce::jlimit (0, kFx - 1, i)];
}

FxBlockNode::FxBlockNode (APVTS_& s)
{
    static const char* const retIds[kFx] = { "choRet", "dlyRet", "revRet" };
    for (int i = 0; i < kFx; ++i)
    {
        setupMiniKnob (ret[i], rowColour (i));
        addAndMakeVisible (ret[i]);
        retA[i] = std::make_unique<APVTS_::SliderAttachment> (s, retIds[i], ret[i]);
    }
}

juce::Rectangle<int> FxBlockNode::rowBounds (int i) const
{
    const int top = 30, h = (getHeight() - top - 8) / kFx;
    return { 8, top + i * h, getWidth() - 16, h - 4 };
}

juce::Point<float> FxBlockNode::inlet (int i) const
{
    return { float (getX() + getWidth() * (i + 1) / (kFx + 1)), (float) getY() };
}

juce::Point<float> FxBlockNode::outlet (int i) const
{
    return { float (getX() + getWidth() * (i + 1) / (kFx + 1)), (float) getBottom() };
}

void FxBlockNode::resized()
{
    for (int i = 0; i < kFx; ++i)
    {
        const auto r = rowBounds (i);
        ret[i].setBounds (r.getRight() - r.getHeight(), r.getY(), r.getHeight(), r.getHeight());
    }
}

void FxBlockNode::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kFx; ++i)
        if (rowBounds (i).contains (e.getPosition())) { if (onSelect) onSelect (i); return; }
    if (onSelect) onSelect (0);
}

void FxBlockNode::paint (juce::Graphics& g)
{
    paintNodeBox (g, getLocalBounds().toFloat(), ui::accent.withAlpha (0.7f), false);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText ("FX (sends)", 10, 6, getWidth() - 20, 20, juce::Justification::centredLeft);

    for (int i = 0; i < kFx; ++i)
    {
        const auto r = rowBounds (i).toFloat();
        const auto c = rowColour (i);
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r, 3.f);
        if (selectedRow == i)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 3.f, 1.5f);
        }
        g.setColour (c);
        g.fillRect (r.withWidth (3.f));
        g.setColour (ui::text);
        g.setFont (juce::FontOptions (12.f));
        g.drawText (rowName (i), r.withTrimmedLeft (10.f), juce::Justification::centredLeft);
        g.setColour (ui::dim);
        g.setFont (juce::FontOptions (10.f));
        g.drawText ("return", r.withTrimmedRight (r.getHeight() + 4.f), juce::Justification::centredRight);
    }
}

MasterNode::MasterNode (APVTS_& s)
{
    setupMiniKnob (gain, ui::accent);
    addAndMakeVisible (gain);
    gainA = std::make_unique<APVTS_::SliderAttachment> (s, "master", gain);
    satOn = s.getRawParameterValue ("satOn");
    limOn = s.getRawParameterValue ("limOn");
    startTimerHz (5);
}

void MasterNode::resized()
{
    gain.setBounds (getWidth() - 50, 30, 40, 40);
}

void MasterNode::paint (juce::Graphics& g)
{
    paintNodeBox (g, getLocalBounds().toFloat(), ui::accent, selected);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText ("Master FX", 10, 6, getWidth() - 20, 20, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.f));
    auto item = [&] (const char* t, bool on, int y)
    {
        const juce::Rectangle<float> r (10.f, (float) y, (float) getWidth() - 70.f, 18.f);
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (on ? ui::accent : ui::outline);
        g.fillRect (r.withWidth (3.f));
        g.setColour (on ? ui::text : ui::dim);
        g.drawText (t, r.withTrimmedLeft (10.f), juce::Justification::centredLeft);
    };
    item ("Saturation", satOn->load() > 0.5f, 30);
    item ("Master gain", true, 52);
    item ("Limiter", limOn->load() > 0.5f, 74);
}

RoutingGraph::RoutingGraph (AuraProcessor& p) : proc (p), fxBlock (p.apvts), master (p.apvts)
{
    for (int l = 0; l < kLayers; ++l)
    {
        auto n = std::make_unique<LayerNode> (p, l);
        n->onSelect = [this, l] { if (onSelect) onSelect (l); };
        n->onEdit = [this, l] { if (onEditLayer) onEditLayer (l); };
        addAndMakeVisible (*n);
        layerNodes.push_back (std::move (n));
    }

    fxBlock.onSelect = [this] (int i) { if (onSelect) onSelect (kLayers + i); };
    addAndMakeVisible (fxBlock);
    master.onSelect = [this] { if (onSelect) onSelect (masterIndex()); };
    addAndMakeVisible (master);
    startTimerHz (10);
}

void RoutingGraph::setSelected (int idx)
{
    for (int l = 0; l < kLayers; ++l) layerNodes[(size_t) l]->selected = (idx == l);
    fxBlock.selectedRow = (idx >= kLayers && idx < kLayers + kFx) ? idx - kLayers : -1;
    master.selected = (idx == masterIndex());
    for (auto& n : layerNodes) n->repaint();
    fxBlock.repaint();
    master.repaint();
}

void RoutingGraph::resized()
{
    const int w = getWidth(), h = getHeight();
    const int nodeW = std::min (180, (w - 40) / kLayers - 16), nodeH = 112;
    const int gap = (w - kLayers * nodeW) / (kLayers + 1);
    for (int l = 0; l < kLayers; ++l)
        layerNodes[(size_t) l]->setBounds (gap + l * (nodeW + gap), 24, nodeW, nodeH);

    // FX a droite du centre, master au centre en bas : les cables dry passent a gauche du bloc FX
    const int fxW = 230, fxH = 150;
    fxBlock.setBounds (w * 62 / 100 - fxW / 2, h * 44 / 100 - fxH / 2, fxW, fxH);

    const int mW = 220, mH = 100;
    master.setBounds (w / 2 - mW / 2, h * 74 / 100 - mH / 2, mW, mH);
    outBox = juce::Rectangle<int> (w / 2 - 40, master.getBottom() + 26, 80, 26);
}

static void drawWire (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour c, float amount)
{
    if (amount < 0.001f) return;
    juce::Path p;
    p.startNewSubPath (a);
    const float dy = std::max (30.f, (b.y - a.y) * 0.5f);
    p.cubicTo (a.x, a.y + dy, b.x, b.y - dy, b.x, b.y);
    g.setColour (c.withAlpha (0.25f + 0.6f * std::min (1.f, amount)));
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
    const auto mIn = juce::Point<float> ((float) master.getBounds().getCentreX(), (float) master.getY());

    for (int l = 0; l < kLayers; ++l)
    {
        if (raw (lpId (l, LP_Enable)) < 0.5f) continue;
        const auto& nb = layerNodes[(size_t) l]->getBounds();
        const auto out = juce::Point<float> ((float) nb.getCentreX(), (float) nb.getBottom());
        const float level = raw (lpId (l, LP_Level));
        drawWire (g, out, mIn, ui::layerColour (l), level);

        const int sendIds[kFx] = { LP_SendCho, LP_SendDly, LP_SendRev };
        for (int i = 0; i < kFx; ++i)
            drawWire (g, out, fxBlock.inlet (i), FxBlockNode::rowColour (i), raw (lpId (l, sendIds[i])) * level);
    }

    static const char* const retIds[kFx] = { "choRet", "dlyRet", "revRet" };
    for (int i = 0; i < kFx; ++i)
        drawWire (g, fxBlock.outlet (i), mIn, FxBlockNode::rowColour (i), raw (retIds[i]));

    // ports d'entree / sortie du bloc FX
    for (int i = 0; i < kFx; ++i)
    {
        g.setColour (FxBlockNode::rowColour (i));
        const auto a = fxBlock.inlet (i), b = fxBlock.outlet (i);
        g.fillEllipse (a.x - 4.f, a.y - 4.f, 8.f, 8.f);
        g.fillEllipse (b.x - 4.f, b.y - 4.f, 8.f, 8.f);
    }

    drawWire (g, { (float) master.getBounds().getCentreX(), (float) master.getBottom() },
              { (float) outBox.getCentreX(), (float) outBox.getY() }, ui::accent, raw ("master"));
    g.setColour (ui::panel2);
    g.fillRoundedRectangle (outBox.toFloat(), 4.f);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (12.f, juce::Font::bold));
    g.drawText ("OUT", outBox, juce::Justification::centred);

    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText ("Click a node to edit it - double-click a layer to open its page. Wire thickness = level / send.",
                8, getHeight() - 20, getWidth() - 16, 16, juce::Justification::centredLeft);
}

RoutingPage::RoutingPage (AuraProcessor& p) : graph (p)
{
    auto& s = p.apvts;
    for (int l = 0; l < kLayers; ++l)
    {
        auto pp = std::make_unique<ParamPanel> (s, "Layer " + juce::String (l + 1), ui::layerColour (l), 3);
        pp->addToggle (lpId (l, LP_Enable), "Enabled");
        pp->addToggle (lpId (l, LP_Arp), "Arpeggiator");
        pp->addToggle (lpId (l, LP_SeqOn), "Step sequencer");
        pp->addKnob (lpId (l, LP_Level), "Level");
        pp->addKnob (lpId (l, LP_Pan), "Pan");
        pp->addKnob (lpId (l, LP_Transpose), "Transpose");
        pp->addKnob (lpId (l, LP_SendCho), "> Chorus");
        pp->addKnob (lpId (l, LP_SendDly), "> Delay");
        pp->addKnob (lpId (l, LP_SendRev), "> Reverb");
        pp->addKnob (lpId (l, LP_KeyLo), "Key Lo");
        pp->addKnob (lpId (l, LP_KeyHi), "Key Hi");
        pp->addKnob (lpId (l, LP_VelLo), "Vel Lo");
        pp->addKnob (lpId (l, LP_VelHi), "Vel Hi");
        pp->setAction ("Edit layer " + juce::String (l + 1), [this, l] { if (onEditLayer) onEditLayer (l); });
        panels.push_back (std::move (pp));
    }

    {
        auto pp = std::make_unique<ParamPanel> (s, "Chorus", juce::Colour (0xff9575cd), 3);
        pp->addKnob ("choRate", "Rate");
        pp->addKnob ("choDepth", "Depth");
        pp->addKnob ("choRet", "Return");
        panels.push_back (std::move (pp));
    }
    {
        auto pp = std::make_unique<ParamPanel> (s, "Delay", juce::Colour (0xff4db6ac), 2);
        pp->addToggle ("dlyPing", "Ping-pong");
        pp->addKnob ("dlyTime", "Time (ms)");
        pp->addKnob ("dlyFb", "Feedback");
        pp->addKnob ("dlyDamp", "Damping");
        pp->addKnob ("dlyRet", "Return");
        panels.push_back (std::move (pp));
    }
    {
        auto pp = std::make_unique<ParamPanel> (s, "Reverb", juce::Colour (0xff7986cb), 3);
        pp->addKnob ("revDecay", "Decay (s)");
        pp->addKnob ("revSize", "Size");
        pp->addKnob ("revDamp", "Damping");
        pp->addKnob ("revPre", "Pre-delay");
        pp->addKnob ("revRet", "Return");
        panels.push_back (std::move (pp));
    }
    {
        auto pp = std::make_unique<ParamPanel> (s, "Master FX", ui::accent, 3);
        pp->addToggle ("satOn", "Saturation");
        pp->addToggle ("limOn", "Limiter");
        pp->addCombo ("satMode", "Sat mode", { "Tanh", "Hard Clip", "Asymmetric" });
        pp->addKnob ("satDrive", "Drive (dB)");
        pp->addKnob ("satMix", "Sat Mix");
        pp->addKnob ("master", "Master");
        pp->addKnob ("limCeil", "Ceiling (dB)");
        pp->addKnob ("limRel", "Release (ms)");
        panels.push_back (std::move (pp));
    }

    for (auto& pp : panels) addChildComponent (*pp);
    addAndMakeVisible (graph);
    graph.onSelect = [this] (int i) { select (i); };
    graph.onEditLayer = [this] (int l) { if (onEditLayer) onEditLayer (l); };
    select (0);
}

void RoutingPage::select (int idx)
{
    selected = juce::jlimit (0, (int) panels.size() - 1, idx);
    for (size_t i = 0; i < panels.size(); ++i) panels[i]->setVisible ((int) i == selected);
    graph.setSelected (selected);
}

void RoutingPage::resized()
{
    auto a = getLocalBounds();
    auto side = a.removeFromRight (std::min (320, a.getWidth() / 3));
    for (auto& pp : panels) pp->setBounds (side.reduced (4));
    graph.setBounds (a.reduced (4));
}

// ============================================================================
// Page FX
// ============================================================================
FxPage::FxPage (AuraProcessor& p)
{
    auto& s = p.apvts;
    auto add = [&] (const juce::String& t, juce::Colour c, int cols)
    {
        panels.push_back (std::make_unique<ParamPanel> (s, t, c, cols));
        addAndMakeVisible (*panels.back());
        return panels.back().get();
    };

    auto* cho = add ("Chorus (send)", FxBlockNode::rowColour (0), 3);
    cho->addKnob ("choRate", "Rate");
    cho->addKnob ("choDepth", "Depth");
    cho->addKnob ("choRet", "Return");

    auto* dly = add ("Delay (send)", FxBlockNode::rowColour (1), 4);
    dly->addToggle ("dlyPing", "Ping-pong");
    dly->addKnob ("dlyTime", "Time (ms)");
    dly->addKnob ("dlyFb", "Feedback");
    dly->addKnob ("dlyDamp", "Damping");
    dly->addKnob ("dlyRet", "Return");

    auto* rev = add ("Reverb (send)", FxBlockNode::rowColour (2), 5);
    rev->addKnob ("revDecay", "Decay (s)");
    rev->addKnob ("revSize", "Size");
    rev->addKnob ("revDamp", "Damping");
    rev->addKnob ("revPre", "Pre-delay");
    rev->addKnob ("revRet", "Return");

    auto* sat = add ("Saturation (master)", ui::accent, 2);
    sat->addToggle ("satOn", "On");
    sat->addCombo ("satMode", "Mode", { "Tanh", "Hard Clip", "Asymmetric" });
    sat->addKnob ("satDrive", "Drive (dB)");
    sat->addKnob ("satMix", "Mix");

    auto* lim = add ("Limiter (master)", ui::accent, 2);
    lim->addToggle ("limOn", "On");
    lim->addKnob ("limCeil", "Ceiling (dB)");
    lim->addKnob ("limRel", "Release (ms)");

    auto* mst = add ("Master", ui::accent, 1);
    mst->addKnob ("master", "Output");
}

void FxPage::resized()
{
    auto a = getLocalBounds();
    auto row1 = a.removeFromTop (a.getHeight() / 2);
    const int w1 = row1.getWidth();
    panels[0]->setBounds (row1.removeFromLeft (w1 * 28 / 100).reduced (4));
    panels[1]->setBounds (row1.removeFromLeft (w1 * 34 / 100).reduced (4));
    panels[2]->setBounds (row1.reduced (4));
    const int w2 = a.getWidth();
    panels[3]->setBounds (a.removeFromLeft (w2 * 40 / 100).reduced (4));
    panels[4]->setBounds (a.removeFromLeft (w2 * 35 / 100).reduced (4));
    panels[5]->setBounds (a.reduced (4));
}

// ============================================================================
// Page ARP
// ============================================================================
ArpPage::ArpPage (AuraProcessor& p) : Section ("Arpeggiator (global)"), knobs (*this, p.apvts)
{
    on.init (*this, p.apvts, "arpon", "Arpeggiator on");
    latch.init (*this, p.apvts, "arplatch", "Latch");
    mode.init (*this, p.apvts, "arpmode", "Mode", { "Up", "Down", "Up/Down", "Random", "As Played", "Chord" });
    rate.init (*this, p.apvts, "arprate", "Rate", kRateItems);
    knobs.add ("arpoct", "Octaves");
    knobs.add ("arpgate", "Gate");
    knobs.add ("arpswing", "Swing");

    hint.setText ("The arpeggiator follows the host tempo and transport. Enable 'Arp' per layer "
                  "(Routing inspector or Zone section); other layers keep playing the raw keys. "
                  "Chain per layer: keys -> Arp -> Step seq -> sound.",
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, ui::dim);
    hint.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (hint);
}

void ArpPage::resized()
{
    auto a = content().reduced (6);
    auto r1 = a.removeFromTop (28);
    on.button.setBounds (r1.removeFromLeft (170));
    latch.button.setBounds (r1.removeFromLeft (100));
    a.removeFromTop (8);
    auto r2 = a.removeFromTop (28);
    mode.place (r2, 50, 130);
    rate.place (r2, 50, 100);
    a.removeFromTop (12);
    knobs.layout (a.removeFromTop (110).removeFromLeft (360), 3);
    a.removeFromTop (12);
    hint.setBounds (a.removeFromTop (60));
}

// ============================================================================
// Colonne des couches
// ============================================================================
LayerRow::LayerRow (AuraProcessor& p, int layerIndex) : proc (p), layer (layerIndex)
{
    addAndMakeVisible (power);
    powerA = std::make_unique<APVTS_::ButtonAttachment> (p.apvts, lpId (layer, LP_Enable), power);

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
    startTimerHz (4);
}

void LayerRow::resized()
{
    power.setBounds (8, 10, 24, 22);
    const int bs = 26, y = getHeight() / 2 - bs / 2;
    solo.setBounds (getWidth() - bs - 8, y, bs, bs);
    mute.setBounds (getWidth() - 2 * bs - 12, y, bs, bs);
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

    const bool on = proc.apvts.getRawParameterValue (lpId (layer, LP_Enable))->load() > 0.5f;
    const int textW = getWidth() - 40 - 66;
    g.setColour (on ? (selected ? c : ui::text) : ui::dim);
    g.setFont (juce::FontOptions (14.f, juce::Font::bold));
    g.drawText (juce::String (layer + 1) + " - Layer " + juce::String (layer + 1), 38, 8, textW, 22,
                juce::Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText (ui::layerTypeText (proc.apvts, layer), 38, 30, textW, 16, juce::Justification::centredLeft);
}

// ============================================================================
// Editeur principal
// ============================================================================
AuraEditor::AuraEditor (AuraProcessor& p)
    : AudioProcessorEditor (&p), proc (p), routing (p), fxPage (p), arpPage (p)
{
    setLookAndFeel (&look);

    static const char* const names[kNumPages] = { "LAYER", "ROUTING", "FX", "ARP" };
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
        r->onSelect = [this, l] { selectLayer (l); showPage (PageLayer); };
        addAndMakeVisible (*r);
        rows.push_back (std::move (r));

        layerEditors.push_back (std::make_unique<LayerEditor> (p, l));
        addChildComponent (*layerEditors.back());
    }
    addChildComponent (routing);
    addChildComponent (fxPage);
    addChildComponent (arpPage);
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
        layerEditors[(size_t) i]->setVisible (page == PageLayer && i == currentLayer);
    routing.setVisible (page == PageRouting);
    fxPage.setVisible (page == PageFx);
    arpPage.setVisible (page == PageArp);
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

    // emplacement du futur browser de presets
    const juce::Rectangle<float> preset (170.f, 10.f, 320.f, 32.f);
    g.setColour (ui::panel2);
    g.fillRoundedRectangle (preset, 5.f);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (14.f));
    g.drawText ("Init", preset.reduced (12.f, 0.f), juce::Justification::centredLeft);
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
        nav[i].setBounds (top.removeFromRight (96).reduced (3, 10));
    }

    // colonne des couches a gauche, edition de la centrale jusqu'au bord droit
    auto column = a.removeFromLeft (230).reduced (6, 8);
    for (auto& r : rows)
    {
        r->setBounds (column.removeFromTop (58));
        column.removeFromTop (4);
    }

    const auto pageArea = a.reduced (4, 6);
    for (auto& e : layerEditors) e->setBounds (pageArea);
    routing.setBounds (pageArea);
    fxPage.setBounds (pageArea);
    arpPage.setBounds (pageArea.withHeight (std::min (pageArea.getHeight(), 320)));
}
