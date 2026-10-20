#pragma once
#include "PluginProcessor.h"

class PedalContent;

class RingEditor : public juce::AudioProcessorEditor
{
public:
    explicit RingEditor (RingProcessor&);
    ~RingEditor() override;
    void resized() override;
    int  getScaleIndex() const;          // 1, 2 = 75 %, 100 %  (-1 = default / free size)
    void setScaleIndex (int idx);        // 1, 2 = 75 %, 100 %  (-1 or 0 = default 67 %, the minimum size)

private:
    RingProcessor& proc;
    std::unique_ptr<PedalContent> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingEditor)
};
