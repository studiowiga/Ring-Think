#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    // Design size = size of background.png (pedal + side margins for the jacks). Every coordinate is in these pixels.
    constexpr int kW = 1240, kH = 928;
    constexpr int kRefW = 993;                       // window sizes (60 / 75 / 100 %) are expressed against this reference width
    const float kKnobX[5] = { 166.0f, 344.0f, 524.5f, 704.0f, 881.0f };
    constexpr float kKnobY = 203.0f, kKnobR = 70.0f;
    constexpr int kBigX = 1057, kBigY = 204;         // big MODE cap (centre pixel of the sprite)
    constexpr int kLadX = 790;
    const int kLadY[9] = { 351, 390, 428, 467, 506, 544, 583, 621, 660 };
    const int kModeLedX[3] = { 1009, 1055, 1103 }, kModeLedY[3] = { 318, 348, 317 };   // RM, UB, LB
    const int kFaceX[3] = { 986, 1034, 1082 };       // button faces (44 x 98) at y = 287
    constexpr int kFaceY = 287, kFaceW = 44, kFaceH = 98;
    const int kLabX[3] = { 990, 1042, 1092 }, kLabY[3] = { 345, 313, 345 };
    constexpr int kPresetLedX = 458, kPresetLedY = 759, kStatusLedX = 766, kStatusLedY = 759;
    constexpr int kFsLX = 256, kFsRX = 967, kFsY = 782;
    constexpr int kPsX = 986, kPsY = 404;            // PITCH-SHIFT label sprite
    constexpr int kPillX = 767, kPillY = 40, kPillW = 130, kPillH = 32;   // PHASER switch
    constexpr int kPhLedX = 787, kPhLedY = 56, kPhTxtX = 805, kPhTxtY = 50;
    const juce::Colour kOrange (0xffff9a1a), kRed (0xffff2a2a), kBtnOrange (0xfff95527), kYellow (0xffffd600);
    const float kScales[3] = { 661.0f / 993.0f, 0.75f, 1.0f };   // index 0 is the default size (the old 50 % slot)
    constexpr float kDefaultScale = 661.0f / 993.0f;      // default window = 661 x 495 px

    // all pictures of the pedal (real photo-style sprites cut out of the artwork)
    struct Art
    {
        Art()
        {
            auto L = [] (const void* d, int n) { return juce::ImageCache::getFromMemory (d, n); };
            bg      = L (BinaryData::background_png,  BinaryData::background_pngSize);
            capBig  = L (BinaryData::cap_big_png,     BinaryData::cap_big_pngSize);
            capL    = L (BinaryData::cap_fsL_png,     BinaryData::cap_fsL_pngSize);
            capR    = L (BinaryData::cap_fsR_png,     BinaryData::cap_fsR_pngSize);
            faceOn  = L (BinaryData::face_on_png,     BinaryData::face_on_pngSize);
            lab[0]  = L (BinaryData::label_rm_png,    BinaryData::label_rm_pngSize);
            lab[1]  = L (BinaryData::label_ub_png,    BinaryData::label_ub_pngSize);
            lab[2]  = L (BinaryData::label_lb_png,    BinaryData::label_lb_pngSize);
            psWhite  = L (BinaryData::pitchshift_white_png,  BinaryData::pitchshift_white_pngSize);
            psYellow = L (BinaryData::pitchshift_yellow_png, BinaryData::pitchshift_yellow_pngSize);
            phWhite  = L (BinaryData::phaser_white_png,  BinaryData::phaser_white_pngSize);
            phYellow = L (BinaryData::phaser_yellow_png, BinaryData::phaser_yellow_pngSize);
            ledOn[0]  = L (BinaryData::led_s_on_png,  BinaryData::led_s_on_pngSize);
            ledOff[0] = L (BinaryData::led_s_off_png, BinaryData::led_s_off_pngSize);
            ledOn[1]  = L (BinaryData::led_p_on_png,  BinaryData::led_p_on_pngSize);
            ledOff[1] = L (BinaryData::led_p_off_png, BinaryData::led_p_off_pngSize);
            ledOn[2]  = L (BinaryData::led_r_on_png,  BinaryData::led_r_on_pngSize);
            ledOff[2] = L (BinaryData::led_r_off_png, BinaryData::led_r_off_pngSize);
        }
        juce::Image bg, capBig, capL, capR, faceOn, lab[3], psWhite, psYellow, phWhite, phYellow, ledOn[3], ledOff[3];
    };

    // draws a sprite whose centre pixel is (cx, cy), optionally scaled around that centre
    void drawSprite (juce::Graphics& g, const juce::Image& im, float cx, float cy, float scale = 1.0f, float alpha = 1.0f)
    {
        const float r = (float) im.getWidth() * 0.5f;
        g.setOpacity (alpha);
        g.drawImageTransformed (im, juce::AffineTransform::translation (-r, -r).scaled (scale).translated (cx + 0.5f, cy + 0.5f));
        g.setOpacity (1.0f);
    }

    // real LED: dark glassy dome when off, cross-faded to the glowing bead + light bloom when lit
    // kind: 0 = small amber, 1 = preset amber, 2 = status red
    void drawLedSprite (juce::Graphics& g, const Art& art, int kind, float cx, float cy, float level, juce::Colour glow, float scale = 1.0f)
    {
        const float r = (float) art.ledOff[kind].getWidth() * 0.5f;
        if (level > 0.01f)
        {
            const float gr = r * 2.15f * scale, gx = cx + 0.5f, gy = cy + 0.5f;
            juce::ColourGradient cg (glow.withAlpha (0.55f * level), gx, gy, glow.withAlpha (0.0f), gx + gr, gy, true);
            cg.addColour (0.35, glow.withAlpha (0.30f * level));
            g.setGradientFill (cg);
            g.fillEllipse (gx - gr, gy - gr, 2 * gr, 2 * gr);
        }
        drawSprite (g, art.ledOff[kind], cx, cy, scale, 1.0f);
        if (level > 0.01f) drawSprite (g, art.ledOn[kind], cx, cy, scale, level);
    }

    void setParamGesture (juce::AudioProcessorValueTreeState& apvts, const char* id, float v, bool begin, bool end)
    {
        if (auto* p = apvts.getParameter (id))
        {
            if (begin) p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            if (end) p->endChangeGesture();
        }
    }
}

static void addSizeSubmenu (juce::PopupMenu&, juce::Component&);
static void handleSizeChoice (int, juce::Component*);
static void showSizeMenuFor (juce::Component&);

// knob that lets a right click open the size menu instead of dragging
struct PedalSlider : juce::Slider
{
    void mouseDown (const juce::MouseEvent& e) override
    {
        passThrough = e.mods.isPopupMenu();
        if (passThrough) showSizeMenuFor (*this); else juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! passThrough) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! passThrough) juce::Slider::mouseUp (e); passThrough = false; }
    bool passThrough = false;
};

// ------------------------------------------------------------------ knob look
struct PedalKnobLNF : juce::LookAndFeel_V4
{
    // the knob itself is part of the artwork (a real, lit black knob); only the white pointer is drawn live
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto c = b.getCentre() + juce::Point<float> (0.5f, 0.5f);
        const float ang = a0 + pos * (a1 - a0);
        const float sx = std::sin (ang), sy = -std::cos (ang);
        const juce::Point<float> p0 (c.x + sx * 15.0f, c.y + sy * 15.0f), p1 (c.x + sx * 49.0f, c.y + sy * 49.0f);
        juce::Path line; line.startNewSubPath (p0); line.lineTo (p1);
        // soft shadow on the knob face, thin dark rim, white moulded pointer, small specular streak
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (line, juce::PathStrokeType (8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (1.2f, 1.8f));
        g.setColour (juce::Colour (0xff050505).withAlpha (0.85f));
        g.strokePath (line, juce::PathStrokeType (7.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xfff2f2f3));
        g.strokePath (line, juce::PathStrokeType (5.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white);
        g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-0.9f, -0.5f));
    }
};

// ------------------------------------------------------------------ big MODE / preset knob
class BigKnob : public juce::Component, private juce::Timer
{
public:
    BigKnob (RingProcessor& p, const Art& a) : proc (p), art (a) {}

    bool isHoldBlinking() const { return holding && ! moved && (juce::Time::getMillisecondCounter() - downTime) > 2000; }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const float cx = 75.0f, cy = 75.0f;                 // centre pixel of the 151 x 151 component
        const float sc = holding ? 0.955f : 1.0f;           // the cap sinks when pushed
        const float rr = 69.5f;
        // contact shadow around the cap (tighter and darker when pushed)
        {
            juce::ColourGradient cg (juce::Colours::black.withAlpha (holding ? 0.90f : 0.55f), cx + 0.5f, cy + (holding ? 1.0f : 3.0f),
                                     juce::Colours::black.withAlpha (0.0f), cx + 0.5f, cy + (holding ? 1.0f : 3.0f) + rr * 1.13f, true);
            cg.addColour (0.86, juce::Colours::black.withAlpha (holding ? 0.90f : 0.55f));
            g.setGradientFill (cg);
            g.fillEllipse (cx + 0.5f - rr * 1.13f, cy + (holding ? 1.0f : 3.0f) - rr * 1.13f, rr * 2.26f, rr * 2.26f);
        }
        drawSprite (g, art.capBig, cx, cy, sc);
        if (holding)
        {
            g.setColour (juce::Colours::black.withAlpha (0.16f));
            g.fillEllipse (cx + 0.5f - rr * sc, cy + 0.5f - rr * sc, 2 * rr * sc, 2 * rr * sc);
        }
        // engraved dot = selected preset (turns with the preset number)
        const int slot = proc.getCurrentSlot();
        const float ang = juce::degreesToRadians (-135.0f + 270.0f * (float) slot / 9.0f);
        const float dx = cx + 0.5f + std::sin (ang) * 41.0f * sc, dy = cy + 0.5f - std::cos (ang) * 41.0f * sc;
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (dx - 5.3f + 0.9f, dy - 5.3f + 1.1f, 10.6f, 10.6f);
        juce::ColourGradient dg (juce::Colour (0xff2a2418), dx - 2.0f, dy - 2.0f, juce::Colour (0xff6d634a), dx + 3.5f, dy + 3.5f, false);
        g.setGradientFill (dg);
        g.fillEllipse (dx - 4.8f, dy - 4.8f, 9.6f, 9.6f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        holding = true; moved = false; accum = 0.0f;
        lastX = e.x; lastY = e.y; downTime = juce::Time::getMillisecondCounter();
        startTimerHz (20);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! holding) return;
        if (e.getDistanceFromDragStart() > 3) moved = true;
        accum += (float) ((e.x - lastX) - (e.y - lastY));
        lastX = e.x; lastY = e.y;
        while (accum >= 14.0f)  { accum -= 14.0f; step (+1); }
        while (accum <= -14.0f) { accum += 14.0f; step (-1); }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! holding) return;
        holding = false; stopTimer();
        const auto dt = juce::Time::getMillisecondCounter() - downTime;
        if (! moved && dt < 500) proc.cycleModes();    // push = change mode
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        step (w.deltaY > 0 ? +1 : -1);
    }

private:
    void step (int d) { proc.loadPreset (juce::jlimit (0, 9, proc.getCurrentSlot() + d)); repaint(); }

    void timerCallback() override
    {
        if (! holding || moved) { stopTimer(); return; }
        if (juce::Time::getMillisecondCounter() - downTime >= 3000)
        {
            holding = false; stopTimer();
            if (proc.getCurrentSlot() > 0) proc.saveUserPreset (proc.getCurrentSlot());   // hold 3 s = save
        }
    }

    void showMenu()
    {
        const int slot = proc.getCurrentSlot();
        juce::PopupMenu m;
        m.addItem (1, slot > 0 ? "Save current sound to preset " + juce::String (slot) : "Save (turn the knob to a preset first)", slot > 0);
        m.addItem (2, slot > 0 ? "Restore factory preset " + juce::String (slot) : "Restore factory preset", slot > 0);
        m.addItem (3, "Restore ALL factory presets");
        m.addSeparator();
        m.addItem (4, "Unload preset (live knob positions)", slot > 0);
        m.addItem (5, "Next mode combination (same as pushing the knob)");
        m.addItem (6, "Load the factory Phaser sound (keeps your 9 presets untouched)");
        addSizeSubmenu (m, *this);
        juce::Component::SafePointer<BigKnob> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) {
            if (sp == nullptr) return;
            auto& p = sp->proc; const int s = p.getCurrentSlot();
            if (r == 1) p.saveUserPreset (s);
            else if (r == 2) p.restoreFactoryPreset (s);
            else if (r == 3) p.restoreAllFactory();
            else if (r == 4) p.loadPreset (0);
            else if (r == 5) p.cycleModes();
            else if (r == 6) p.loadFactorySound (9);
            else handleSizeChoice (r, sp.getComponent());
        });
    }

    RingProcessor& proc;
    const Art& art;
    bool holding = false, moved = false;
    float accum = 0.0f; int lastX = 0, lastY = 0;
    juce::uint32 downTime = 0;
};

// ------------------------------------------------------------------ invisible click zones
class ToggleZone : public juce::Component
{
public:
    ToggleZone (RingProcessor& p, const char* paramId) : proc (p), id (paramId) {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showSizeMenuFor (*this); return; }
        const float cur = proc.apvts.getRawParameterValue (id)->load();
        setParamGesture (proc.apvts, id, cur > 0.5f ? 0.0f : 1.0f, true, true);
    }
    void paint (juce::Graphics&) override {}
private:
    RingProcessor& proc; const char* id;
};

class LadderZone : public juce::Component
{
public:
    explicit LadderZone (RingProcessor& p) : proc (p) {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showSizeMenuFor (*this); return; }
        const int y = e.y + getY();
        int best = 0, bd = 100000;
        for (int i = 0; i < 9; ++i) if (std::abs (y - kLadY[i]) < bd) { bd = std::abs (y - kLadY[i]); best = i; }
        proc.loadPreset (best + 1);
    }
    void paint (juce::Graphics&) override {}
private:
    RingProcessor& proc;
};

// ------------------------------------------------------------------ footswitches
class Footswitch : public juce::Component
{
public:
    Footswitch (RingProcessor& p, const Art& a, bool presetSwitch) : proc (p), art (a), isPreset (presetSwitch) {}
    ~Footswitch() override
    {
        if (down && isPreset) setParamGesture (proc.apvts, "presetfsw", 0.0f, false, true);   // never leave the switch stuck
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const float cx = 65.0f, cy = 65.0f;               // centre pixel of the 131 x 131 component
        const float sc = down ? 0.945f : 1.0f;            // the cap sinks into its ring when pressed
        const float rr = 40.5f;
        drawSprite (g, isPreset ? art.capL : art.capR, cx, cy, sc);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.20f));
            g.fillEllipse (cx + 0.5f - rr * sc, cy + 0.5f - rr * sc, 2 * rr * sc, 2 * rr * sc);
        }
        if (proc.getLearnTarget() == target() && (juce::Time::getMillisecondCounter() / 300) % 2 == 0)
        {
            g.setColour (kOrange);
            g.drawEllipse (cx + 0.5f - rr - 6, cy + 0.5f - rr - 6, 2 * rr + 12, 2 * rr + 12, 3.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        down = true;
        if (isPreset) setParamGesture (proc.apvts, "presetfsw", 1.0f, true, false);
        else
        {
            const float cur = proc.apvts.getRawParameterValue ("active")->load();
            setParamGesture (proc.apvts, "active", cur > 0.5f ? 0.0f : 1.0f, true, true);
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! down) return;
        down = false;
        if (isPreset) setParamGesture (proc.apvts, "presetfsw", 0.0f, false, true);
        repaint();
    }

private:
    MidiTarget target() const { return isPreset ? MidiTarget::PresetFsw : MidiTarget::Bypass; }

    void showMenu()
    {
        const auto desc = proc.getMidiDescription (target());
        juce::PopupMenu m;
        m.addItem (1, "MIDI learn  (then move a pedal / press a note)");
        m.addItem (2, "Clear MIDI assignment", desc.isNotEmpty());
        m.addSeparator();
        m.addItem (3, desc.isNotEmpty() ? "Assigned to: " + desc : "No MIDI assignment", false);
        if (proc.getLearnTarget() == target()) m.addItem (5, "Cancel MIDI learn");
        if (! isPreset) m.addItem (6, "Latching MIDI switch (toggle on press AND release)", true, proc.isBypassLatching());
        m.addItem (7, "MIDI Program Change 1-9 also selects presets (0 = live knobs)", false);
        addSizeSubmenu (m, *this);
        m.addItem (4, "Tip: in your DAW you can also map the automatable parameter \""
                          + juce::String (isPreset ? "Preset/Tune Footswitch" : "Effect On (Bypass switch)") + "\"", false);
        juce::Component::SafePointer<Footswitch> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) {
            if (sp == nullptr) return;
            if (r == 1) sp->proc.startMidiLearn (sp->target());
            if (r == 2) sp->proc.clearMidi (sp->target());
            if (r == 5) sp->proc.cancelMidiLearn();
            if (r == 6) sp->proc.setBypassLatching (! sp->proc.isBypassLatching());
            handleSizeChoice (r, sp.getComponent());
        });
    }

    RingProcessor& proc;
    const Art& art;
    bool isPreset, down = false;
};

// ------------------------------------------------------------------ whole pedal
class PedalContent : public juce::Component, private juce::Timer
{
public:
    explicit PedalContent (RingProcessor& p)
        : proc (p), bigKnob (p, art), rmZone (p, "rm"), ubZone (p, "ub"), lbZone (p, "lb"), psZone (p, "ps"), phZone (p, "ph"),
          ladder (p), presetSw (p, art, true), bypassSw (p, art, false)
    {
        setSize (kW, kH);
        for (auto& l : lvl) l = 0.0f;

        juce::Slider* ks[5] = { &blend, &wave, &filter, &fine, &coarse };
        const char* ids[5] = { "blend", "wave", "filter", "fine", "coarse" };
        const char* labels[5] = { "BLEND", "WAVE", "FILTER/RATE", "FINE/DEPTH", "COARSE" };
        for (int i = 0; i < 5; ++i)
        {
            auto& s = *ks[i];
            s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            s.setMouseDragSensitivity (240);
            s.setLookAndFeel (&lnf);
            addAndMakeVisible (s);
            s.setBounds ((int) (kKnobX[i] - kKnobR), (int) (kKnobY - kKnobR), (int) (2 * kKnobR), (int) (2 * kKnobR));
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], s));
            s.onValueChange = [this, &s, name = juce::String (labels[i])]
            {
                const int pct = (int) std::lround (s.valueToProportionOfLength (s.getValue()) * 100.0);
                knobText = name + "   " + juce::String (pct) + " %";
                knobTextMs = juce::Time::getMillisecondCounter();
            };
            s.setDoubleClickReturnValue (true, proc.apvts.getParameter (ids[i])->convertFrom0to1 (proc.apvts.getParameter (ids[i])->getDefaultValue()));
        }

        addAndMakeVisible (bigKnob);  bigKnob.setBounds (kBigX - 75, kBigY - 75, 151, 151);
        addAndMakeVisible (rmZone);   rmZone.setBounds (kFaceX[0], kFaceY, kFaceW, kFaceH);
        addAndMakeVisible (ubZone);   ubZone.setBounds (kFaceX[1], kFaceY, kFaceW, kFaceH);
        addAndMakeVisible (lbZone);   lbZone.setBounds (kFaceX[2], kFaceY, kFaceW, kFaceH);
        addAndMakeVisible (psZone);   psZone.setBounds (kPsX - 6, kPsY - 5, 154, 32);
        addAndMakeVisible (phZone);   phZone.setBounds (kPillX, kPillY, kPillW, kPillH);
        addAndMakeVisible (ladder);   ladder.setBounds (751, 331, 218, 354);
        addAndMakeVisible (presetSw); presetSw.setBounds (kFsLX - 65, kFsY - 65, 131, 131);
        addAndMakeVisible (bypassSw); bypassSw.setBounds (kFsRX - 65, kFsY - 65, 131, 131);
        startTimerHz (30);
    }

    ~PedalContent() override
    {
        for (auto* s : { &blend, &wave, &filter, &fine, &coarse }) s->setLookAndFeel (nullptr);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) showSizeMenuFor (*this);
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (art.bg, getLocalBounds().toFloat());

        const auto now = juce::Time::getMillisecondCounter();
        const int modes = proc.getModes();

        // mode buttons: the orange face lights up (with a soft halo), the label stays white
        for (int i = 0; i < 3; ++i)
        {
            const float L = lvl[11 + i];
            if (L > 0.01f)
            {
                g.setColour (kBtnOrange.withAlpha (0.16f * L));
                g.fillRoundedRectangle ((float) kFaceX[i] - 5.0f, (float) kFaceY - 5.0f, (float) kFaceW + 10.0f, (float) kFaceH + 10.0f, 4.0f);
                g.setOpacity (L);
                g.drawImageAt (art.faceOn, kFaceX[i], kFaceY);
                g.setOpacity (1.0f);
            }
            g.drawImageAt (art.lab[i], kLabX[i], kLabY[i]);
        }
        for (int i = 0; i < 3; ++i)
            drawLedSprite (g, art, 0, (float) kModeLedX[i], (float) kModeLedY[i], lvl[11 + i], kOrange);

        // PITCH-SHIFT label: yellow = on, white = off (the label lives in two sprites, not in the background)
        g.drawImageAt ((modes & kPS) ? art.psYellow : art.psWhite, kPsX, kPsY);

        // PHASER switch (top of the body): engraved pill, LED + label, yellow when on
        const bool phOn = (modes & kPH) != 0;
        g.drawImageAt (phOn ? art.phYellow : art.phWhite, kPhTxtX, kPhTxtY);
        drawLedSprite (g, art, 0, (float) kPhLedX, (float) kPhLedY, lvl[14], kOrange, 0.62f);

        // preset ladder LEDs
        for (int i = 0; i < 9; ++i)
            drawLedSprite (g, art, 0, (float) kLadX, (float) kLadY[i], lvl[i], kOrange);

        // PRESET LED: solid = preset loaded, fast blink = knobs moved since loading, slow blink = tuning
        drawLedSprite (g, art, 1, (float) kPresetLedX, (float) kPresetLedY, lvl[9], kOrange);
        drawLedSprite (g, art, 2, (float) kStatusLedX, (float) kStatusLedY, lvl[10], kRed);

        // small status line
        juce::String status;
        if (proc.isTunedActive()) status << "TUNED " << juce::String (proc.getTunedHz(), 1) << " Hz   ";
        const auto m1 = proc.getMidiDescription (MidiTarget::PresetFsw), m2 = proc.getMidiDescription (MidiTarget::Bypass);
        if (m1.isNotEmpty()) status << "MIDI PRESET/TUNE: " << m1 << "   ";
        if (m2.isNotEmpty()) status << "MIDI BYPASS: " << m2;
        if (proc.getLearnTarget() != MidiTarget::None) status = "MIDI LEARN: move a control / press a note...";
        else if (knobText.isNotEmpty() && (now - knobTextMs) < 1500) status = knobText;   // live knob percentage
        if (status.isNotEmpty())
        {
            const juce::Rectangle<float> r (330.0f, 806.0f, 520.0f, 24.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (juce::Colour (0xffdcdcdc));
            g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            g.drawText (status, r.toNearestInt(), juce::Justification::centred);
        }
    }

private:
    // light level of every LED eased towards its target: fast attack, short afterglow like a real bulb
    void updateLevels()
    {
        const auto now = juce::Time::getMillisecondCounter();
        const int modes = proc.getModes();
        const int slot = proc.getCurrentSlot();
        const bool dirty = proc.isPresetDirty();
        const bool saveFlash = (now - proc.lastSaveMs) < 900 && ((now / 60) % 2 == 0);
        const bool holdBlink = bigKnob.isHoldBlinking() && ((now / 60) % 2 == 0);

        bool presetLit = slot > 0;
        if (slot > 0 && dirty) presetLit = (now / 120) % 2 == 0;
        if (proc.isTuning()) presetLit = (now / 250) % 2 == 0;

        bool target[15];
        for (int i = 0; i < 9; ++i) target[i] = (i + 1 == slot) || saveFlash || holdBlink;
        target[9]  = presetLit;
        target[10] = proc.apvts.getRawParameterValue ("active")->load() > 0.5f;
        target[11] = (modes & kRM) != 0;
        target[12] = (modes & kUB) != 0;
        target[13] = (modes & kLB) != 0;
        target[14] = (modes & kPH) != 0;
        for (int i = 0; i < 15; ++i)
            lvl[i] += (target[i] ? 1.0f - lvl[i] : -lvl[i]) * (target[i] ? 0.70f : 0.42f);
    }

    void timerCallback() override { updateLevels(); repaint(); }

    RingProcessor& proc;
    Art art;
    PedalKnobLNF lnf;
    PedalSlider blend, wave, filter, fine, coarse;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::String knobText;                // e.g. "BLEND  63 %" shown in the status line while a knob moves
    juce::uint32 knobTextMs = 0;
    float lvl[15];
    BigKnob bigKnob;
    ToggleZone rmZone, ubZone, lbZone, psZone, phZone;
    LadderZone ladder;
    Footswitch presetSw, bypassSw;
};

// ------------------------------------------------------------------ editor
RingEditor::RingEditor (RingProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<PedalContent> (proc);
    addAndMakeVisible (*content);
    setResizable (true, true);
    const int minW = (int) std::lround (kRefW * kDefaultScale);                 // never smaller than the default size
    const int maxW = 1500;
    setResizeLimits (minW, (int) std::lround ((double) minW * kH / kW), maxW, (int) std::lround ((double) maxW * kH / kW));
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setScaleIndex (juce::jlimit (-1, 2, proc.uiScaleIdx.load()));   // default: 67 %
}

RingEditor::~RingEditor() = default;

void RingEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setTransform (juce::AffineTransform::scale (s));
    content->setBounds (0, 0, kW, kH);
}

int RingEditor::getScaleIndex() const
{
    const float s = (float) getWidth() / (float) kRefW;
    for (int i = 1; i < 3; ++i) if (std::abs (s - kScales[i]) < 0.02f) return i;
    return -1;
}

void RingEditor::setScaleIndex (int idx)
{
    idx = juce::jlimit (-1, 2, idx);
    if (idx == 0) idx = -1;                                   // the old 50 % slot is now the default size
    proc.uiScaleIdx = idx;
    const float s = idx < 0 ? kDefaultScale : kScales[idx];
    const int w = (int) std::lround (kRefW * s);
    setSize (w, (int) std::lround ((double) w * kH / kW));
}

static bool isDefaultSize (const RingEditor* ed)
{
    return ed != nullptr && std::abs ((float) ed->getWidth() / (float) kRefW - kDefaultScale) < 0.02f;
}

static void addSizeSubmenu (juce::PopupMenu& m, juce::Component& c)
{
    const auto* ed = c.findParentComponentOfClass<RingEditor>();
    const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
    juce::PopupMenu sub;
    sub.addItem (104, "67 % (default)", true, isDefaultSize (ed));
    sub.addItem (102, "75 %", true, cur == 1);
    sub.addItem (103, "100 %", true, cur == 2);
    m.addSeparator();
    m.addSubMenu ("Plugin size", sub);
}

static void handleSizeChoice (int r, juce::Component* c)
{
    if (c == nullptr || r < 102 || r > 104) return;
    if (auto* ed = c->findParentComponentOfClass<RingEditor>()) ed->setScaleIndex (r == 104 ? -1 : r - 101);
}

static void showSizeMenuFor (juce::Component& c)
{
    const auto* ed = c.findParentComponentOfClass<RingEditor>();
    const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
    juce::PopupMenu m;
    m.addSectionHeader ("Plugin size");
    m.addItem (104, "67 % (default)", true, isDefaultSize (ed));
    m.addItem (102, "75 %", true, cur == 1);
    m.addItem (103, "100 %", true, cur == 2);
    juce::Component::SafePointer<juce::Component> sp (&c);
    m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { handleSizeChoice (r, sp.getComponent()); });
}
