#pragma once
#include "PluginProcessor.h"

class PedalContent;

class RingEditor : public juce::AudioProcessorEditor
{
public:
    explicit RingEditor (RingProcessor&);
    ~RingEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    int  getScaleIndex() const;          // 1, 2 = 75 %, 100 %  (-1 = default / free size)
    void setScaleIndex (int idx);        // 1, 2 = 75 %, 100 %  (-1 or 0 = default 67 %, the minimum size)

private:
    friend class PedalContent;           // son timer surveille le moteur de rendu (anti-gel v2)
    void useSoftwareRenderer();          // applique le moteur logiciel (Windows) si la fenêtre a changé
    void checkRenderer();                // depuis le timer : seulement si la fenêtre a changé ou au plus 1 fois / s
    void resetRenderer();                // chien de garde : oublie la fenêtre mémorisée et réapplique tout
    void* lastPeer = nullptr;
    juce::uint32 lastRendererCheck = 0;
    RingProcessor& proc;
    std::unique_ptr<PedalContent> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingEditor)
};
