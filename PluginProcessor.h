#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

enum ModeBits { kRM = 1, kUB = 2, kLB = 4, kPS = 8, kPH = 16 };
enum class MidiTarget { None = 0, PresetFsw = 1, Bypass = 2 };

struct PresetData
{
    float blend = 0.6f, wave = 0.25f, filter = 0.8f, fine = 0.5f, coarse = 0.8f;   // = factory "Ring Mod"
    int modes = kRM;
    float tunedHz = 0.0f;
};

class RingProcessor : public juce::AudioProcessor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::Timer
{
public:
    RingProcessor();
    ~RingProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Ring Think"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Host-visible factory programs (the preset menu of the DAW)
    int getNumPrograms() override { return 10; }      // 9 preset slots + the factory Phaser sound
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return factoryNames[juce::jlimit (0, 9, index)]; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- presets ----
    static const char* const factoryNames[10];
    static PresetData factoryPreset (int index0to9);   // 0-8 = preset slots 1-9, 9 = Phaser
    int  getCurrentSlot() const { return currentSlot; }
    void loadPreset (int slot0to9);          // 0 = unload (live knobs)
    void loadFactorySound (int index0to9);   // applies a factory sound without touching the 9 user slots
    void saveUserPreset (int slot1to9);
    void restoreFactoryPreset (int slot1to9);
    void restoreAllFactory();
    bool isPresetDirty() const;
    void requestNextPreset();
    void cycleModes();
    int  getModes() const;
    juce::uint32 lastSaveMs = 0;

    // ---- tune / midi ----
    bool  isTuning() const { return tuning.load(); }
    bool  isTunedActive() const { return tunedActive.load(); }
    float getTunedHz() const { return tunedHz.load(); }
    void startMidiLearn (MidiTarget t) { learnTarget = t == MidiTarget::PresetFsw ? lPresetFsw : (t == MidiTarget::Bypass ? lBypass : -1); }
    void cancelMidiLearn() { learnTarget = -1; }
    bool isBypassLatching() const { return bypassLatching.load(); }
    void setBypassLatching (bool b) { bypassLatching = b; }
    std::atomic<int> uiScaleIdx { -1 };       // -1 = default size (67 %), 0..2 = 50 / 75 / 100 %
    MidiTarget getLearnTarget() const
    {
        const int lt = learnTarget.load();
        return lt == lPresetFsw ? MidiTarget::PresetFsw : (lt == lBypass ? MidiTarget::Bypass : MidiTarget::None);
    }
    void clearMidi (MidiTarget t);
    juce::String getMidiDescription (MidiTarget t) const;

    // v2.4 MIDI Learn : toutes les commandes de la facade (clic droit sur la commande)
    // les 5 potards et les 5 boutons de mode sont aussi l'ordre de la file anti-gel (pendingValue / pendingSet)
    enum LearnType { learnContinuous, learnToggle, learnSelector, learnFootswitch };
    struct LearnDef { const char* id; const char* name; int defaultCC; int type; int positions; };
    enum { lPresetKnob = 10, lPresetFsw = 11, lBypass = 12 };
    static constexpr int numLearn = 13;
    static const LearnDef& learnDef (int i);
    std::atomic<int> ccMap[numLearn];            // numero de CC de chaque commande, -1 = non assigne
    std::atomic<int> learnTarget { -1 };         // commande en attente d'un CC (-1 = pas de learn)
    std::atomic<int> lastLearned { -1 };         // derniere commande apprise (lue et remise a -1 par la fenetre)
    void resetMidiMap();
    void clearLearn (int i);
    juce::String learnAssignText (int i) const;  // "CC n", "Note n" ou "none"

    void setParamValue (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct AP {
        float a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float process (float x) { float y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
    };
    struct Chan {
        AP p1[4], p2[4];
        float z1 = 0, l1 = 0, l2 = 0, l3 = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;   // 60 Hz, message thread: does every host call queued by the audio thread
    bool handleMidi (juce::MidiBuffer&);   // returns true if a PRESET/TUNE press arrived in this block
    float detectPitch();
    void applyPresetData (const PresetData&);
    PresetData captureCurrent() const;
    static juce::File getPresetFile();
    void loadUserPresetsFromDisk();
    void writeUserPresetsToDisk();

    Chan chans[2];
    double sr = 44100.0;
    float carrierPhase = 0.f, lfoPhase = 0.f;
    // pitch shifter: two crossfaded read taps, window locked to a multiple of the played note's period
    float psPh[2] = { 0.f, 0.5f }, psW[2] = { 1920.f, 1920.f };
    float psTargetW = 1920.f;
    int psHold = 0;
    std::vector<float> psBuf[2];
    int psMask = 0, psPos = 0, psWin = 1024;

    juce::SmoothedValue<float> freqSm, blendSm, cutSm, psBaseSm, depthSm, activeSm;

    static constexpr int tuneSize = 4096;
    std::vector<float> tuneBuf, tuneWork, yinD;
    int tuneWrite = 0, tuneCounter = 0;

    std::atomic<float> *pBlend, *pWave, *pFilter, *pFine, *pCoarse, *pRM, *pUB, *pLB, *pPS, *pPH, *pActive, *pFsw, *pPreset;

    bool prevDown = false;
    double holdSamples = 0.0;
    std::atomic<bool> tuning { false }, tunedActive { false };
    std::atomic<float> tunedHz { 0.f };
    float tunedCoarse = 0.f, tunedFine = 0.f;

    std::atomic<bool> midiPresetDown { false }, midiBypassHeld { false }, bypassLatching { false };
    float phS[2][6] = {};
    float phFb[2] = { 0.f, 0.f };
    float phLfoPhase = 0.f;
    // footswitches : CC dans ccMap[lPresetFsw] / ccMap[lBypass], ou note MIDI
    std::atomic<int> notePreset { -1 }, noteBypass { -1 };

    // anti-gel : valeurs envoyees par le MIDI (thread audio), transmises a l'hote par le timer
    static constexpr int numQueued = 10;
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];
    std::atomic<float>* queuedPtr[numQueued] {};
    float eff (int q) const   // valeur en file (MIDI pas encore transmis) sinon valeur du parametre
    {
        return pendingSet[q].load (std::memory_order_acquire) ? pendingValue[q].load (std::memory_order_relaxed)
                                                               : queuedPtr[q]->load (std::memory_order_relaxed);
    }

    std::atomic<bool> pendingNext { false }, pendingToggle { false };
    std::atomic<int> pendingPreset { -1 }, pendingProgram { -1 };
    int currentSlot = 0, currentProgram = 0;
    juce::uint32 programLockStart = 0;
    PresetData userPresets[9];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingProcessor)
};
