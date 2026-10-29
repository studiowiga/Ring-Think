#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    const double hA1[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
    const double hA2[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };

    float polyBlep (float t, float dt)
    {
        if (t < dt)         { t /= dt;               return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)  { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // Five waveforms crossfaded by one knob: square > sine > ramp down | ramp up > triangle
    float morphWave (float ph, float dt, float w)
    {
        dt = juce::jlimit (0.00001f, 0.49f, dt);
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float sine = std::sin (twoPi * ph);
        if (w < 0.25f)
        {
            float sq = ph < 0.5f ? 1.0f : -1.0f;
            float ph2 = ph + 0.5f; ph2 -= std::floor (ph2);
            sq += polyBlep (ph, dt) - polyBlep (ph2, dt);
            const float t = w * 4.0f;
            return sq * (1.0f - t) + sine * t;
        }
        if (w < 0.5f)
        {
            const float rd = 1.0f - 2.0f * ph + polyBlep (ph, dt);
            const float t = (w - 0.25f) * 4.0f;
            return sine * (1.0f - t) + rd * t;
        }
        const float ru = 2.0f * ph - 1.0f - polyBlep (ph, dt);
        const float tri = 4.0f * std::abs (ph - 0.5f) - 1.0f;
        const float t = (w - 0.5f) * 2.0f;
        return ru * (1.0f - t) + tri * t;
    }
}

const char* const RingProcessor::factoryNames[10] = {
    "Ring Mod", "Tremolo", "Ring Vibe", "Detune", "Chorus", "Vibrato", "Sub Octave", "Octave Up", "Rotary Thing", "Phaser" };

PresetData RingProcessor::factoryPreset (int i)
{
    //                       blend  wave   filter fine   coarse modes
    static const PresetData t[10] = {
        { 0.60f, 0.25f, 0.80f, 0.50f, 0.80f, kRM, 0.0f },   // Ring Mod
        { 0.50f, 0.25f, 1.00f, 0.50f, 0.38f, kRM, 0.0f },   // Tremolo
        { 0.50f, 0.50f, 1.00f, 0.50f, 0.35f, kLB, 0.0f },   // Ring Vibe
        { 0.50f, 0.25f, 0.00f, 0.60f, 0.50f, kPS, 0.0f },   // Detune
        { 0.50f, 0.25f, 0.30f, 0.15f, 0.50f, kPS, 0.0f },   // Chorus
        { 1.00f, 0.25f, 0.50f, 0.25f, 0.50f, kPS, 0.0f },   // Vibrato
        { 0.60f, 0.25f, 0.00f, 0.50f, 0.25f, kPS, 0.0f },   // Sub Octave (-12 st)
        { 0.50f, 0.25f, 0.00f, 0.50f, 0.75f, kPS, 0.0f },   // Octave Up (+12 st)
        { 0.70f, 0.25f, 0.50f, 0.35f, 0.50f, kPS, 0.0f },   // Rotary Thing
        { 0.50f, 0.25f, 0.45f, 0.50f, 0.50f, kPH, 0.0f } }; // Phaser (6-stage all-pass sweep) - host program 10
    return t[juce::jlimit (0, 9, i)];
}

const RingProcessor::LearnDef& RingProcessor::learnDef (int i)
{
    // aucun CC par defaut (les footswitches n'avaient pas de numero avant le learn : inchanges)
    static const LearnDef defs[numLearn] =
    {
        { "blend",     "Blend",                 -1, learnContinuous, 0 },
        { "wave",      "Wave",                  -1, learnContinuous, 0 },
        { "filter",    "Filter/Rate",           -1, learnContinuous, 0 },
        { "fine",      "Fine/Depth",            -1, learnContinuous, 0 },
        { "coarse",    "Coarse",                -1, learnContinuous, 0 },
        { "rm",        "RM button",             -1, learnToggle,     2 },   // >= 64 : on
        { "ub",        "UB button",             -1, learnToggle,     2 },
        { "lb",        "LB button",             -1, learnToggle,     2 },
        { "ps",        "Pitch-Shift switch",    -1, learnToggle,     2 },
        { "ph",        "Phaser switch",         -1, learnToggle,     2 },
        { "preset",    "Preset knob",           -1, learnSelector,  10 },   // 0 = live knobs, 1-9 = presets
        { "presetfsw", "Preset/Tune footswitch",-1, learnFootswitch, 0 },
        { "active",    "Bypass footswitch",     -1, learnFootswitch, 0 },
    };
    return defs[juce::jlimit (0, numLearn - 1, i)];
}

void RingProcessor::resetMidiMap()
{
    learnTarget = -1;
    for (int i = 0; i < numLearn; ++i) ccMap[i].store (learnDef (i).defaultCC);
    notePreset = -1; noteBypass = -1; midiPresetDown = false;
}

void RingProcessor::clearLearn (int i)
{
    if (i < 0 || i >= numLearn) return;
    ccMap[i].store (-1);
    if (i == lPresetFsw) { notePreset = -1; midiPresetDown = false; }
    if (i == lBypass)    noteBypass = -1;
    if (learnTarget.load() == i) learnTarget = -1;
}

juce::String RingProcessor::learnAssignText (int i) const
{
    const int cc = ccMap[juce::jlimit (0, numLearn - 1, i)].load();
    if (cc >= 0) return "CC " + juce::String (cc);
    const int note = i == lPresetFsw ? notePreset.load() : (i == lBypass ? noteBypass.load() : -1);
    if (note >= 0) return "Note " + juce::String (note);
    return "none";
}

juce::AudioProcessorValueTreeState::ParameterLayout RingProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto f = [&p] (const char* id, const char* name, float def) {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def));
    };
    f ("blend", "Blend", 0.6f);          // defaults = factory "Ring Mod" sound
    f ("wave", "Wave", 0.25f);
    f ("filter", "Filter/Rate", 0.8f);
    f ("fine", "Fine/Depth", 0.5f);
    f ("coarse", "Coarse", 0.8f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "rm", 1 }, "Mode RM", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "ub", 1 }, "Mode UB", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "lb", 1 }, "Mode LB", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "ps", 1 }, "Mode Pitch Shift", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "ph", 1 }, "Mode Phaser", false));
    p.push_back (std::make_unique<AudioParameterInt>  (ParameterID { "preset", 1 }, "Preset", 0, 9, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "Effect On (Bypass switch)", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "presetfsw", 1 }, "Preset/Tune Footswitch", false));
    return { p.begin(), p.end() };
}

RingProcessor::RingProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pBlend = apvts.getRawParameterValue ("blend");   pWave = apvts.getRawParameterValue ("wave");
    pFilter = apvts.getRawParameterValue ("filter"); pFine = apvts.getRawParameterValue ("fine");
    pCoarse = apvts.getRawParameterValue ("coarse"); pRM = apvts.getRawParameterValue ("rm");
    pUB = apvts.getRawParameterValue ("ub");         pLB = apvts.getRawParameterValue ("lb");
    pPS = apvts.getRawParameterValue ("ps");         pPH = apvts.getRawParameterValue ("ph");
    pActive = apvts.getRawParameterValue ("active");
    pFsw = apvts.getRawParameterValue ("presetfsw"); pPreset = apvts.getRawParameterValue ("preset");
    for (int i = 0; i < numQueued; ++i)
    {
        queuedPtr[i] = apvts.getRawParameterValue (learnDef (i).id);
        pendingValue[i].store (0.0f); pendingSet[i].store (false);
    }
    resetMidiMap();

    programLockStart = juce::Time::getMillisecondCounter();
    for (int i = 0; i < 9; ++i) userPresets[i] = factoryPreset (i);
    loadUserPresetsFromDisk();
    apvts.addParameterListener ("preset", this);
    startTimerHz (60);   // anti-gel v2: no host call is ever made from the audio thread
}

RingProcessor::~RingProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("preset", this);
}

bool RingProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
         || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void RingProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& c : chans)
    {
        c = Chan();
        for (int k = 0; k < 4; ++k) { c.p1[k].a2 = (float) (hA1[k] * hA1[k]); c.p2[k].a2 = (float) (hA2[k] * hA2[k]); }
    }
    for (auto& row : phS) for (auto& v : row) v = 0.f;
    phFb[0] = phFb[1] = 0.f; phLfoPhase = 0.f;
    midiPresetDown = false; midiBypassHeld = false; prevDown = false; holdSamples = 0.0; tuning = false;
    psWin = juce::jmax (256, (int) (0.04 * sr));
    // window up to psWin + 2 periods of the lowest tracked note (40 Hz)
    const int size = juce::nextPowerOfTwo (psWin + 2 * (int) (sr / 40.0) + 64);
    for (auto& b : psBuf) b.assign ((size_t) size, 0.0f);
    psMask = size - 1; psPos = 0;
    psPh[0] = 0.0f; psPh[1] = 0.5f; psW[0] = psW[1] = psTargetW = (float) psWin; psHold = 0;

    tuneBuf.assign ((size_t) tuneSize, 0.0f);
    tuneWork.assign ((size_t) tuneSize, 0.0f);
    yinD.assign ((size_t) 3100, 0.0f);
    tuneWrite = 0; tuneCounter = 0;

    freqSm.reset (sr, 0.03);  freqSm.setCurrentAndTargetValue (10.0f);
    blendSm.reset (sr, 0.02); blendSm.setCurrentAndTargetValue (pBlend->load());
    cutSm.reset (sr, 0.02);   cutSm.setCurrentAndTargetValue (20000.0f);
    psBaseSm.reset (sr, 0.03); psBaseSm.setCurrentAndTargetValue (0.0f);
    depthSm.reset (sr, 0.03);  depthSm.setCurrentAndTargetValue (0.0f);
    activeSm.reset (sr, 0.01); activeSm.setCurrentAndTargetValue (pActive->load() > 0.5f ? 1.0f : 0.0f);
}

int RingProcessor::getModes() const
{
    return (pRM->load() > 0.5f ? kRM : 0) | (pUB->load() > 0.5f ? kUB : 0)
         | (pLB->load() > 0.5f ? kLB : 0) | (pPS->load() > 0.5f ? kPS : 0) | (pPH->load() > 0.5f ? kPH : 0);
}

// ------------------------------------------------------------------ MIDI
bool RingProcessor::handleMidi (juce::MidiBuffer& midi)
{
    bool pressed = false;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();

        // MIDI Learn : le premier CC recu (ou une note pour les footswitches) est affecte a la commande visee
        const int lt = learnTarget.load();
        if (lt >= 0 && lt < numLearn)
        {
            const bool isFs = lt == lPresetFsw || lt == lBypass;
            if (m.isController() && m.getControllerNumber() < 120)
            {
                const int cc = m.getControllerNumber();
                for (int j = 0; j < numLearn; ++j)
                    if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);   // numero deja pris : l'autre commande est desassignee
                ccMap[lt].store (cc);
                if (lt == lPresetFsw) notePreset = -1;
                if (lt == lBypass)    { noteBypass = -1; midiBypassHeld = m.getControllerValue() >= 64; }
                lastLearned.store (lt);
                learnTarget.store (-1);
                continue;
            }
            if (isFs && m.isNoteOn())
            {
                const int note = m.getNoteNumber();
                if (lt == lPresetFsw) { notePreset = note; if (noteBypass.load() == note) noteBypass = -1; }
                else                  { noteBypass = note; if (notePreset.load() == note) notePreset = -1; }
                ccMap[lt].store (-1);
                lastLearned.store (lt);
                learnTarget.store (-1);
                continue;
            }
        }

        if (m.isProgramChange())
        {
            pendingPreset = juce::jlimit (0, 9, m.getProgramChangeNumber());   // PC 0 = live knobs, 1-9 = presets
        }
        else if (m.isController() && m.getControllerNumber() < 120)
        {
            const int c = m.getControllerNumber();
            const int v = m.getControllerValue();
            const bool on = v >= 64;
            for (int j = 0; j < numLearn; ++j)
            {
                if (ccMap[j].load (std::memory_order_relaxed) != c) continue;
                if (j < 5)                                       // potards : 0-127 -> 0-1 (lissage du moteur)
                {
                    pendingValue[j].store ((float) v / 127.0f, std::memory_order_relaxed);
                    pendingSet[j].store (true, std::memory_order_release);
                }
                else if (j < 10)                                 // boutons de mode : >= 64 = on
                {
                    pendingValue[j].store (on ? 1.0f : 0.0f, std::memory_order_relaxed);
                    pendingSet[j].store (true, std::memory_order_release);
                }
                else if (j == lPresetKnob)                       // 10 positions : live knobs, presets 1 a 9
                {
                    pendingPreset = juce::jmin (9, v * 10 / 128);
                }
                else if (j == lPresetFsw) { midiPresetDown = on; if (on) pressed = true; }
                else if (j == lBypass)
                {
                    const bool edge = bypassLatching.load() ? (on != midiBypassHeld.load()) : (on && ! midiBypassHeld.load());
                    if (edge) { pendingToggle = true; }
                    midiBypassHeld = on;
                }
            }
        }
        else if (m.isNoteOn())
        {
            if (m.getNoteNumber() == notePreset.load()) { midiPresetDown = true; pressed = true; }
            if (m.getNoteNumber() == noteBypass.load()) { pendingToggle = true; }
        }
        else if (m.isNoteOff())
        {
            if (m.getNoteNumber() == notePreset.load()) midiPresetDown = false;
        }
    }
    return pressed;
}

void RingProcessor::clearMidi (MidiTarget t)
{
    clearLearn (t == MidiTarget::PresetFsw ? lPresetFsw : lBypass);
}

juce::String RingProcessor::getMidiDescription (MidiTarget t) const
{
    const auto d = learnAssignText (t == MidiTarget::PresetFsw ? lPresetFsw : lBypass);
    return d == "none" ? juce::String() : d;
}

// ------------------------------------------------------------------ pitch detection (YIN)
float RingProcessor::detectPitch()
{
    const int N = tuneSize, W = 1024;
    const int tauMax = juce::jmin ((int) (sr / 40.0), 3000, N - W - 1);
    const int tauMin = juce::jmax (2, (int) (sr / 1500.0));
    if (tauMax <= tauMin + 2) return 0.0f;

    for (int i = 0; i < N; ++i) tuneWork[(size_t) i] = tuneBuf[(size_t) ((tuneWrite - N + i) & (N - 1))];
    const float* x = tuneWork.data() + (N - W - tauMax);

    double energy = 0.0;
    for (int j = 0; j < W; ++j) energy += (double) x[tauMax + j] * x[tauMax + j];
    if (std::sqrt (energy / W) < 0.003) return 0.0f;

    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        const float* a = x + tauMax;      // newest window
        const float* b = x + tauMax - tau;
        for (int j = 0; j < W; ++j) { const float d = a[j] - b[j]; sum += (double) d * d; }
        yinD[(size_t) tau] = (float) sum;
    }
    // cumulative mean normalised difference
    double running = 0.0;
    std::vector<float>& d = yinD;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += d[(size_t) tau];
        d[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
    }
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (d[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
            best = tau; break;
        }
    }
    if (best < 0) return 0.0f;
    float t = (float) best;
    if (best > 1 && best < tauMax)
    {
        const float s0 = d[(size_t) best - 1], s1 = d[(size_t) best], s2 = d[(size_t) best + 1];
        const float den = 2.0f * (2.0f * s1 - s2 - s0);
        if (std::abs (den) > 1e-9f) t += (s2 - s0) / den;
    }
    const float f0 = (float) sr / t;
    return (f0 >= 30.0f && f0 <= 1500.0f) ? f0 : 0.0f;
}

// ------------------------------------------------------------------ audio
void RingProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    const int nCh = juce::jmin (buf.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buf.clear (c, 0, n);

    const bool midiPressed = handleMidi (midi);

    // PRESET/TUNE footswitch: short press = next preset, hold >= 1 s = continuous tune
    // (prevDown = "switch currently held"). A tap whose press AND release land in the same audio
    // block is still detected, because the press event itself is reported by handleMidi().
    const bool downNow = pFsw->load() > 0.5f || midiPresetDown.load();
    if (! prevDown && (downNow || midiPressed)) { prevDown = true; holdSamples = 0.0; tuning = false; }
    if (prevDown)
    {
        if (downNow)
        {
            holdSamples += n;
            if (! tuning.load() && holdSamples >= sr) tuning = true;
        }
        else
        {
            if (! tuning.load()) { pendingNext = true; }
            prevDown = false; holdSamples = 0.0; tuning = false;
        }
    }

    const bool active = pActive->load() > 0.5f;
    activeSm.setTargetValue (active ? 1.0f : 0.0f);
    if (! active && ! activeSm.isSmoothing()) return;

    // potards : valeur MIDI encore en file prise tout de suite (sans attendre le timer), puis lissage habituel
    const float blend = eff (0), wave = eff (1), filter = eff (2);
    const float fine = eff (3), coarse = eff (4);
    const int modes = getModes();
    const bool rm = (modes & kRM) != 0, ub = (modes & kUB) != 0, lb = (modes & kLB) != 0, ps = (modes & kPS) != 0;
    const bool ph = (modes & kPH) != 0;
    const bool ssb = ub || lb, ringAny = rm || ssb;

    if (tunedActive.load() && (std::abs (coarse - tunedCoarse) > 1e-3f || std::abs (fine - tunedFine) > 1e-3f))
        tunedActive = false;

    const float fHz = tunedActive.load() ? tunedHz.load()
                                         : 0.1f * std::pow (23900.0f, coarse) * (1.0f + (fine - 0.5f) * 0.66f);
    freqSm.setTargetValue (juce::jlimit (0.05f, 4000.0f, fHz));
    blendSm.setTargetValue (blend);
    const float cutoff = juce::jmin (20.0f * std::pow (1000.0f, filter), (float) (0.45 * sr));
    cutSm.setTargetValue (cutoff);
    const bool lpfOn = filter < 0.985f;

    const float lfoRate = (ps && filter > 0.02f) ? 0.1f * std::pow (5000.0f, (filter - 0.02f) / 0.98f) : 0.0f;
    const bool modOn = lfoRate > 0.0f;
    psBaseSm.setTargetValue (std::round ((coarse - 0.5f) * 48.0f) + (modOn ? 0.0f : (fine - 0.5f) * 2.0f));
    depthSm.setTargetValue (modOn ? fine * 1.5f : 0.0f);

    const float phRate = 0.05f * std::pow (200.0f, filter);            // 0.05 .. 10 Hz
    const float phCenter = 200.0f * std::pow (20.0f, coarse);          // 200 Hz .. 4 kHz
    const float phDepth = fine;                                         // 0 .. +/-2 octaves

    float w2 = 0.0f, w3 = 0.0f;
    if (wave < 0.5f) w2 = (0.5f - wave) * 2.0f; else w3 = (wave - 0.5f) * 2.0f;
    const float hNorm = 1.0f / (1.0f + 0.5f * (w2 + w3));

    float* ch[2] = { buf.getWritePointer (0), nCh > 1 ? buf.getWritePointer (1) : nullptr };
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float fs = (float) sr;

    for (int s = 0; s < n; ++s)
    {
        const float f = freqSm.getNextValue();
        const float b = blendSm.getNextValue();
        const float g = 1.0f - std::exp (-twoPi * cutSm.getNextValue() / fs);
        const float act = activeSm.getNextValue();
        const float baseS = psBaseSm.getNextValue();
        const float dep = depthSm.getNextValue();

        float mono = 0.0f;
        for (int c = 0; c < nCh; ++c) mono += ch[c][s];
        tuneBuf[(size_t) (tuneWrite & (tuneSize - 1))] = mono / (float) nCh;
        ++tuneWrite;

        const float dt = f / fs;
        carrierPhase += dt; carrierPhase -= std::floor (carrierPhase);
        lfoPhase += lfoRate / fs; lfoPhase -= std::floor (lfoPhase);

        const float rmCar = rm ? morphWave (carrierPhase, dt, wave) : 0.0f;
        float cH = 0.0f, sH = 0.0f;
        if (ssb)
        {
            const float a = twoPi * carrierPhase;
            cH = (std::cos (a) + w2 * std::cos (2.0f * a) + w3 * std::cos (3.0f * a)) * hNorm;
            sH = (std::sin (a) + w2 * std::sin (2.0f * a) + w3 * std::sin (3.0f * a)) * hNorm;
        }

        float aPh = 0.0f;
        if (ph)
        {
            phLfoPhase += phRate / fs; phLfoPhase -= std::floor (phLfoPhase);
            const float l = morphWave (phLfoPhase, phRate / fs, wave);
            const float fc = juce::jlimit (40.0f, 0.45f * fs, phCenter * std::pow (2.0f, 2.0f * phDepth * l));
            const float tn = std::tan (juce::MathConstants<float>::pi * fc / fs);
            aPh = (tn - 1.0f) / (tn + 1.0f);
        }

        float g1 = 0.0f, g2 = 0.0f, d1 = 2.0f, d2 = 2.0f;
        if (ps)
        {
            const float lfoV = modOn ? morphWave (lfoPhase, lfoRate / fs, wave) : 0.0f;
            const float ratio = std::pow (2.0f, (baseS + dep * lfoV) / 12.0f);
            const float slip = 1.0f - ratio;
            float d[2], gg[2];
            for (int k = 0; k < 2; ++k)
            {
                psPh[k] += slip / psW[k];
                const bool wrapped = psPh[k] >= 1.0f || psPh[k] < 0.0f;
                if (wrapped)
                {
                    // this tap is silent here: take the new window, and re-lock it half a cycle
                    // away from the other tap so both read whole periods apart (no phase cancelling)
                    psPh[k] -= std::floor (psPh[k]);
                    psW[k] = psTargetW;
                    float o = psPh[1 - k] + 0.5f; o -= std::floor (o);
                    float e = psPh[k] - o; e -= std::floor (e + 0.5f);
                    if (std::abs (e) < 0.15f) psPh[k] = o;
                }
                d[k]  = 2.0f + psPh[k] * psW[k];
                gg[k] = 0.5f - 0.5f * std::cos (twoPi * psPh[k]);
            }
            const float norm = 1.0f / juce::jmax (0.3f, gg[0] + gg[1]);
            d1 = d[0]; d2 = d[1]; g1 = gg[0] * norm; g2 = gg[1] * norm;
        }

        for (int c = 0; c < nCh; ++c)
        {
            auto& st = chans[c];
            const float x = ch[c][s];

            float ringSum = 0.0f; int nRing = 0;
            if (rm) { ringSum += x * rmCar; ++nRing; }
            if (ssb)
            {
                float a = x, q = x;
                for (int k = 0; k < 4; ++k) { a = st.p1[k].process (a); q = st.p2[k].process (q); }
                const float i = st.z1; st.z1 = a;
                if (ub) { ringSum += i * cH - q * sH; ++nRing; }
                if (lb) { ringSum += i * cH + q * sH; ++nRing; }
            }
            float ring = nRing > 0 ? ringSum / (float) nRing : 0.0f;
            if (ringAny && lpfOn)
            {
                st.l1 += g * (ring - st.l1);
                st.l2 += g * (st.l1 - st.l2);
                st.l3 += g * (st.l2 - st.l3);
                ring = st.l3;
            }

            float psOut = 0.0f;
            auto& pb = psBuf[c];
            pb[(size_t) psPos] = x;
            if (ps)
            {
                auto rd = [&] (float delay) {
                    const float rp = (float) psPos - delay;
                    const int i0 = (int) std::floor (rp);
                    const float fr = rp - (float) i0;
                    const float va = pb[(size_t) (i0 & psMask)], vb = pb[(size_t) ((i0 + 1) & psMask)];
                    return va + fr * (vb - va);
                };
                psOut = rd (d1) * g1 + rd (d2) * g2;
            }

            float phOut = 0.0f;
            if (ph)
            {
                float v = x + 0.4f * phFb[c];
                for (int k = 0; k < 6; ++k)
                {
                    const float y = aPh * v + phS[c][k];
                    phS[c][k] = v - aPh * y;
                    v = y;
                }
                phFb[c] = v; phOut = v;
            }

            const int count = (ringAny ? 1 : 0) + (ps ? 1 : 0) + (ph ? 1 : 0);
            const float wet = count == 0 ? x : (ring + psOut + phOut) / std::sqrt ((float) count);
            const float processed = x * (1.0f - b) + wet * b;
            ch[c][s] = x + (processed - x) * act;
        }
        psPos = (psPos + 1) & psMask;
    }

    // pitch tracking: locks the pitch-shifter window to the played note (clean sub / octave up),
    // and feeds the continuous tuning of the ring-modulator carrier
    const bool ringTune = tuning.load() && (modes & (kRM | kUB | kLB)) != 0;
    if (ps || ringTune)
    {
        tuneCounter += n;
        if (tuneCounter >= 1024)
        {
            const int elapsed = tuneCounter;
            tuneCounter = 0;
            const float f0 = detectPitch();
            if (ps)
            {
                if (f0 > 0.0f)
                {
                    const float T = (float) sr / f0;
                    const float m = std::ceil ((float) psWin / (2.0f * T));
                    psTargetW = juce::jlimit ((float) psWin, (float) (psMask - 8), 2.0f * m * T);
                    psHold = (int) (0.3 * sr);
                }
                else if (psHold > 0)
                {
                    psHold -= elapsed;
                    if (psHold <= 0) psTargetW = (float) psWin;   // chords / silence: fixed window
                }
            }
            if (ringTune && f0 > 0.0f)
            {
                tunedHz = f0; tunedActive = true;
                tunedCoarse = coarse; tunedFine = fine;
            }
        }
    }
}

// ------------------------------------------------------------------ presets
void RingProcessor::setParamValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

PresetData RingProcessor::captureCurrent() const
{
    PresetData d;
    d.blend = pBlend->load(); d.wave = pWave->load(); d.filter = pFilter->load();
    d.fine = pFine->load();   d.coarse = pCoarse->load(); d.modes = getModes();
    d.tunedHz = tunedActive.load() ? tunedHz.load() : 0.0f;
    return d;
}

void RingProcessor::applyPresetData (const PresetData& p)
{
    setParamValue ("blend", p.blend);  setParamValue ("wave", p.wave);   setParamValue ("filter", p.filter);
    setParamValue ("fine", p.fine);    setParamValue ("coarse", p.coarse);
    setParamValue ("rm", (p.modes & kRM) ? 1.0f : 0.0f); setParamValue ("ub", (p.modes & kUB) ? 1.0f : 0.0f);
    setParamValue ("lb", (p.modes & kLB) ? 1.0f : 0.0f); setParamValue ("ps", (p.modes & kPS) ? 1.0f : 0.0f);
    setParamValue ("ph", (p.modes & kPH) ? 1.0f : 0.0f);
    tunedCoarse = p.coarse; tunedFine = p.fine;
    tunedHz = p.tunedHz;
    tunedActive = p.tunedHz > 0.0f;
}

void RingProcessor::loadPreset (int slot)
{
    slot = juce::jlimit (0, 9, slot);
    currentSlot = slot;
    setParamValue ("preset", (float) slot);
    if (slot > 0) applyPresetData (userPresets[slot - 1]);
}

void RingProcessor::requestNextPreset()
{
    loadPreset (currentSlot >= 9 ? 0 : currentSlot + 1);   // after preset 9: live knobs, then preset 1 again
}

bool RingProcessor::isPresetDirty() const
{
    if (currentSlot < 1) return false;
    const auto& p = userPresets[currentSlot - 1];
    const auto c = captureCurrent();
    auto d = [] (float a, float b) { return std::abs (a - b) > 0.002f; };
    return d (c.blend, p.blend) || d (c.wave, p.wave) || d (c.filter, p.filter)
        || d (c.fine, p.fine) || d (c.coarse, p.coarse) || c.modes != p.modes;
}

void RingProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9) return;
    userPresets[slot - 1] = captureCurrent();
    writeUserPresetsToDisk();
    lastSaveMs = juce::Time::getMillisecondCounter();
}

void RingProcessor::restoreFactoryPreset (int slot)
{
    if (slot < 1 || slot > 9) return;
    userPresets[slot - 1] = factoryPreset (slot - 1);
    writeUserPresetsToDisk();
    if (currentSlot == slot) applyPresetData (userPresets[slot - 1]);
}

void RingProcessor::restoreAllFactory()
{
    for (int i = 0; i < 9; ++i) userPresets[i] = factoryPreset (i);
    writeUserPresetsToDisk();
    if (currentSlot > 0) applyPresetData (userPresets[currentSlot - 1]);
}

void RingProcessor::cycleModes()
{
    static const int order[] = { kRM, kUB, kLB, kRM | kUB, kUB | kLB, kRM | kLB, kRM | kUB | kLB, kPS, kPH };
    const int cur = getModes();
    int next = order[0];
    for (int i = 0; i < 9; ++i) if (order[i] == cur) { next = order[(i + 1) % 9]; break; }
    setParamValue ("rm", (next & kRM) ? 1.0f : 0.0f); setParamValue ("ub", (next & kUB) ? 1.0f : 0.0f);
    setParamValue ("lb", (next & kLB) ? 1.0f : 0.0f); setParamValue ("ps", (next & kPS) ? 1.0f : 0.0f);
    setParamValue ("ph", (next & kPH) ? 1.0f : 0.0f);
}

juce::File RingProcessor::getPresetFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("WigaRingThink");
    dir.createDirectory();
    return dir.getChildFile ("user_presets.xml");
}

void RingProcessor::loadUserPresetsFromDisk()
{
    auto f = getPresetFile();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::parseXML (f))
        for (auto* e : xml->getChildIterator())
        {
            const int slot = e->getIntAttribute ("slot", 0);
            if (slot < 1 || slot > 9) continue;
            auto& p = userPresets[slot - 1];
            p.blend = (float) e->getDoubleAttribute ("blend", p.blend);
            p.wave = (float) e->getDoubleAttribute ("wave", p.wave);
            p.filter = (float) e->getDoubleAttribute ("filter", p.filter);
            p.fine = (float) e->getDoubleAttribute ("fine", p.fine);
            p.coarse = (float) e->getDoubleAttribute ("coarse", p.coarse);
            p.modes = e->getIntAttribute ("modes", p.modes);
            p.tunedHz = (float) e->getDoubleAttribute ("tunedHz", 0.0);
        }
}

void RingProcessor::writeUserPresetsToDisk()
{
    juce::XmlElement root ("PRESETS");
    for (int i = 0; i < 9; ++i)
    {
        auto* e = root.createNewChildElement ("P");
        const auto& p = userPresets[i];
        e->setAttribute ("slot", i + 1);
        e->setAttribute ("blend", (double) p.blend); e->setAttribute ("wave", (double) p.wave);
        e->setAttribute ("filter", (double) p.filter); e->setAttribute ("fine", (double) p.fine);
        e->setAttribute ("coarse", (double) p.coarse); e->setAttribute ("modes", p.modes);
        e->setAttribute ("tunedHz", (double) p.tunedHz);
    }
    getPresetFile().replaceWithText (root.toString());
}

void RingProcessor::parameterChanged (const juce::String& id, float v)
{
    if (id == "preset") { pendingPreset = (int) std::lround (v); }
}

void RingProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index > 9) return;
    // some hosts re-send a program number while a project is being restored: never let that overwrite the saved sound
    if (juce::Time::getMillisecondCounter() - programLockStart < 2500) return;
    currentProgram = index;
    pendingProgram = index;
}

void RingProcessor::loadFactorySound (int index)
{
    if (index < 0 || index > 9) return;
    currentProgram = index;
    pendingProgram = index;
}

void RingProcessor::timerCallback()
{
    // anti-gel : valeurs MIDI des potards et boutons transmises a l'hote ici (thread de messages)
    for (int i = 0; i < numQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = apvts.getParameter (learnDef (i).id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (pendingValue[i].load (std::memory_order_relaxed)));
    if (pendingToggle.exchange (false))
        setParamValue ("active", pActive->load() > 0.5f ? 0.0f : 1.0f);
    if (pendingNext.exchange (false))
        requestNextPreset();
    const int prog = pendingProgram.exchange (-1);
    if (prog >= 0)
    {
        currentSlot = 0; setParamValue ("preset", 0.0f);
        applyPresetData (factoryPreset (prog));
    }
    const int v = pendingPreset.exchange (-1);
    if (v >= 0 && v != currentSlot) loadPreset (v);
}

// ------------------------------------------------------------------ state
juce::AudioProcessorEditor* RingProcessor::createEditor() { return new RingEditor (*this); }

void RingProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    apvts.state.setProperty ("ccPreset", ccMap[lPresetFsw].load(), nullptr);      // anciennes cles (projets < 2.4)
    apvts.state.setProperty ("notePreset", notePreset.load(), nullptr);
    apvts.state.setProperty ("ccBypass", ccMap[lBypass].load(), nullptr);
    for (int i = 0; i < numLearn; ++i)
        apvts.state.setProperty (juce::String ("cc_") + learnDef (i).id, ccMap[i].load(), nullptr);
    apvts.state.setProperty ("noteBypass", noteBypass.load(), nullptr);
    apvts.state.setProperty ("tunedHz", (double) tunedHz.load(), nullptr);
    apvts.state.setProperty ("tunedActive", tunedActive.load(), nullptr);
    apvts.state.setProperty ("uiScaleIdx", uiScaleIdx.load(), nullptr);
    apvts.state.setProperty ("bypassLatching", bypassLatching.load(), nullptr);
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RingProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // assignations MIDI Learn (-1 = non assigne ; projet < 2.4 : seulement ccPreset / ccBypass)
            learnTarget = -1;
            for (int i = 0; i < numLearn; ++i)
            {
                const juce::var legacy = i == lPresetFsw ? apvts.state.getProperty ("ccPreset", -1)
                                       : i == lBypass    ? apvts.state.getProperty ("ccBypass", -1)
                                                         : juce::var (learnDef (i).defaultCC);
                ccMap[i].store (juce::jlimit (-1, 127, (int) apvts.state.getProperty (juce::String ("cc_") + learnDef (i).id, legacy)));
            }
            notePreset = (int) apvts.state.getProperty ("notePreset", -1);
            noteBypass = (int) apvts.state.getProperty ("noteBypass", -1);
            tunedHz = (float) (double) apvts.state.getProperty ("tunedHz", 0.0);
            tunedActive = (bool) apvts.state.getProperty ("tunedActive", false);
            tunedCoarse = pCoarse->load(); tunedFine = pFine->load();
            currentSlot = (int) std::lround (pPreset->load());
            programLockStart = juce::Time::getMillisecondCounter();
            uiScaleIdx = juce::jlimit (-1, 2, (int) apvts.state.getProperty ("uiScaleIdx", -1));
            bypassLatching = (bool) apvts.state.getProperty ("bypassLatching", false);
            midiPresetDown = false; midiBypassHeld = false; tuning = false;
            setParamValue ("presetfsw", 0.0f);                // never restore a stuck footswitch
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RingProcessor(); }
