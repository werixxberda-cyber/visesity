#include "PluginEditor.h"

using juce::Colour;

namespace
{
    const Colour kPink   (0xffff2e93);
    const Colour kOrange (0xffff9a3c);
    const Colour kCyan   (0xff29f0ff);
    const Colour kBlue   (0xff3a6bff);
    const Colour kDeep   (0xff12062b);

    juce::Font mkFont (float h, int style) { return juce::Font (juce::FontOptions (h, style)); }
}

ViceVoxAudioProcessorEditor::ViceVoxAudioProcessorEditor (ViceVoxAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setSize (720, 420);

    for (auto* s : { &reverbKnob, &delayKnob, &outKnob })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s->setColour (juce::Slider::rotarySliderFillColourId, kPink);
        s->setColour (juce::Slider::rotarySliderOutlineColourId, Colour (0xff3a1a6e));
        s->setColour (juce::Slider::thumbColourId, Colour (0xffffe14d));
        s->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*s);
    }

    aRev = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "reverb", reverbKnob);
    aDel = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "delay", delayKnob);
    aOut = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "out", outKnob);

    startTimerHz (30);
}

void ViceVoxAudioProcessorEditor::resized()
{
    btnUnder = { 85, 116, 260, 78 };
    btnFlint = { 375, 116, 260, 78 };
    const int kw = 110, kh = 128, ky = 270;
    reverbKnob.setBounds (170, ky, kw, kh);
    delayKnob.setBounds (305, ky, kw, kh);
    outKnob.setBounds (440, ky, kw, kh);
}

void ViceVoxAudioProcessorEditor::setMode (int newMode)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("mode")))
    {
        const int target = (p->getIndex() == newMode) ? 0 : newMode;   // click again = off
        p->beginChangeGesture();
        *p = target;
        p->endChangeGesture();
    }
}

void ViceVoxAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (btnUnder.contains (e.getPosition())) setMode (1);
    else if (btnFlint.contains (e.getPosition())) setMode (2);
}

void ViceVoxAudioProcessorEditor::drawPalm (juce::Graphics& g, float x, float baseY, float hgt, float lean)
{
    g.setColour (Colour (0xff0b0220));
    juce::Path trunk;
    trunk.startNewSubPath (x, baseY);
    trunk.quadraticTo (x + lean * 0.4f, baseY - hgt * 0.5f, x + lean, baseY - hgt);
    g.strokePath (trunk, juce::PathStrokeType (7.0f));

    const float tx = x + lean, ty = baseY - hgt, len = hgt * 0.42f;
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < 7; ++i)
    {
        const float a = pi + pi * (float) i / 6.0f;
        const float ex = tx + std::cos (a) * len;
        const float ey = ty + std::sin (a) * len * 0.5f + len * 0.45f;
        juce::Path frond;
        frond.startNewSubPath (tx, ty);
        frond.quadraticTo (tx + std::cos (a) * len * 0.5f, ty + std::sin (a) * len * 0.95f, ex, ey);
        g.strokePath (frond, juce::PathStrokeType (3.5f));
    }
}

void ViceVoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int w = getWidth(), h = getHeight();
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const float lvl = juce::jlimit (0.0f, 1.0f, proc.level.load());
    phase = std::fmod (phase + 0.02f, 1000.0f);

    // ---- sky
    juce::ColourGradient sky (kDeep, 0.0f, 0.0f, kOrange, 0.0f, 300.0f, false);
    sky.addColour (0.45, Colour (0xff5b1a8f));
    sky.addColour (0.75, kPink);
    g.setGradientFill (sky);
    g.fillRect (0, 0, w, 300);

    // ---- sun (pulses with the output level)
    const float cx = (float) w * 0.5f, cy = 190.0f, r = 118.0f + lvl * 12.0f;
    {
        juce::ColourGradient glow (kPink.withAlpha (0.45f), cx, cy, kPink.withAlpha (0.0f), cx, cy + r * 1.8f, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - r * 1.8f, cy - r * 1.8f, r * 3.6f, r * 3.6f);

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (0, 0, w, 300);
        for (int i = 0; i < 6; ++i)
        {
            const int yy = (int) (cy + 8.0f + (float) i * 15.0f);
            g.excludeClipRegion (juce::Rectangle<int> ((int) (cx - r - 2), yy, (int) (r * 2 + 4), 2 + i * 2));
        }
        g.setGradientFill (juce::ColourGradient (Colour (0xffffe14d), cx, cy - r, kPink, cx, cy + r, false));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    // ---- floor + neon grid
    g.setGradientFill (juce::ColourGradient (Colour (0xff2a0a5e), 0.0f, 300.0f, Colour (0xff0a0220), 0.0f, (float) h, false));
    g.fillRect (0, 300, w, h - 300);
    g.setColour (kPink.withAlpha (0.35f));
    for (int i = 0; i < 8; ++i)
    {
        const float t = ((float) i + std::fmod (phase, 1.0f)) / 8.0f;
        g.drawHorizontalLine ((int) (300.0f + t * t * (float) (h - 300)), 0.0f, (float) w);
    }
    for (int i = -10; i <= 10; ++i)
        g.drawLine (cx + (float) i * 12.0f, 300.0f, cx + (float) i * 80.0f, (float) h, 1.0f);
    g.setColour (kPink);
    g.fillRect (0, 299, w, 2);

    // ---- palms
    drawPalm (g, 70.0f, 300.0f, 200.0f, 30.0f);
    drawPalm (g, (float) w - 70.0f, 300.0f, 170.0f, -26.0f);

    // ---- title (neon)
    g.setFont (mkFont (54.0f, juce::Font::bold | juce::Font::italic));
    const juce::Rectangle<int> tr (0, 14, w, 64);
    g.setColour (kCyan.withAlpha (0.85f));  g.drawText ("VICE VOX", tr.translated (-3, 2), juce::Justification::centred);
    g.setColour (kPink);                    g.drawText ("VICE VOX", tr.translated (3, -1), juce::Justification::centred);
    g.setColour (juce::Colours::white);     g.drawText ("VICE VOX", tr, juce::Justification::centred);
    g.setFont (mkFont (13.0f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText ("V O C A L   C H A I N   //   F L   S T U D I O", 0, 78, w, 18, juce::Justification::centred);

    // ---- big buttons
    auto drawBtn = [&] (juce::Rectangle<int> r2, const juce::String& title, const juce::String& sub,
                        bool on, Colour c1, Colour c2)
    {
        auto rf = r2.toFloat();
        if (on)
        {
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (c1.withAlpha (0.10f));
                g.fillRoundedRectangle (rf.expanded ((float) i * 3.0f), 20.0f + (float) i * 3.0f);
            }
            g.setGradientFill (juce::ColourGradient (c1, rf.getX(), rf.getY(), c2, rf.getX(), rf.getBottom(), false));
            g.fillRoundedRectangle (rf, 20.0f);
        }
        else
        {
            g.setColour (Colour (0xdd12062b));
            g.fillRoundedRectangle (rf, 20.0f);
            g.setColour (c1.withAlpha (0.85f));
            g.drawRoundedRectangle (rf.reduced (1.0f), 20.0f, 2.0f);
        }
        g.setColour (on ? juce::Colours::white : c1);
        g.setFont (mkFont (28.0f, juce::Font::bold | juce::Font::italic));
        g.drawText (title, r2.withTrimmedBottom (18), juce::Justification::centred);
        g.setFont (mkFont (11.0f, juce::Font::bold));
        g.setColour (on ? juce::Colours::white.withAlpha (0.9f) : juce::Colours::white.withAlpha (0.55f));
        g.drawText (sub, r2.withTop (r2.getBottom() - 26), juce::Justification::centred);
    };
    drawBtn (btnUnder, "UNDERGROUND", "ALL STAGES  2-7", mode == 1, kPink, kOrange);
    drawBtn (btnFlint, "FLINT", "EQ + COMP + DE-ESS + SAT  3-6", mode == 2, kCyan, kBlue);

    // ---- stage chips
    const char* names[6] = { "2 AUTO-TUNE", "3 EQ", "4 COMP", "5 DE-ESS", "6 SAT", "7 REV + DLY" };
    const bool act[6] = { mode == 1, mode >= 1, mode >= 1, mode >= 1, mode >= 1, mode == 1 };
    const int cw = 104, gap = 6, cx0 = (w - (6 * cw + 5 * gap)) / 2;
    g.setFont (mkFont (11.0f, juce::Font::bold));
    for (int i = 0; i < 6; ++i)
    {
        const auto rc = juce::Rectangle<float> ((float) (cx0 + i * (cw + gap)), 214.0f, (float) cw, 26.0f);
        if (act[i])
        {
            g.setGradientFill (juce::ColourGradient (kPink, rc.getX(), rc.getY(), kOrange, rc.getRight(), rc.getY(), false));
            g.fillRoundedRectangle (rc, 13.0f);
            g.setColour (juce::Colours::white);
        }
        else
        {
            g.setColour (Colour (0xaa12062b));
            g.fillRoundedRectangle (rc, 13.0f);
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawRoundedRectangle (rc.reduced (0.5f), 13.0f, 1.0f);
        }
        g.drawText (names[i], rc.toNearestInt(), juce::Justification::centred);
    }

    // ---- knob labels
    g.setFont (mkFont (12.0f, juce::Font::bold));
    g.setColour (kCyan);
    g.drawText ("REVERB", reverbKnob.getBounds().withY (252).withHeight (18), juce::Justification::centred);
    g.drawText ("DELAY",  delayKnob.getBounds().withY (252).withHeight (18),  juce::Justification::centred);
    g.drawText ("OUTPUT", outKnob.getBounds().withY (252).withHeight (18),    juce::Justification::centred);
}
