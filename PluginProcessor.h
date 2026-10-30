#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

// ---------------------------------------------------------------
//  Son : modélisation d'une corde de guitare électrique (Karplus-Strong
//  étendu : retard fractionnaire, filtre de boucle, médiator, micro)
// ---------------------------------------------------------------
struct GuitarSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class GuitarVoice : public juce::SynthesiserVoice
{
public:
    explicit GuitarVoice (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<GuitarSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Random rng;
    std::vector<float> buffer;
    int   delayLen = 2, pos = 0, pickupOffset = 1;
    float tapWeight = 0.7f;
    float eta = 0.f, x1 = 0.f, y1 = 0.f, lp = 0.f, a = 0.3f, g = 0.999f;
    float level = 1.f, relCoef = 0.999f, gain = 0.5f;
    bool  releasing = false;
    int   silentCount = 0;
};

// ---------------------------------------------------------------
class TaLaVieGuitareProcessor : public juce::AudioProcessor
{
public:
    TaLaVieGuitareProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TaLaVie Guitare"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::Synthesiser synth;
    juce::Reverb reverb;
    using Filter = juce::dsp::IIR::Filter<float>;
    Filter hp[2], mid[2], lpf[2];
    juce::dsp::IIR::Coefficients<float>::Ptr hpC[2], midC[2], lpC[2];
    int lastVoicing = -1;
    juce::dsp::Chorus<float> chorusFx;
    std::vector<float> delayBuf[2];
    int delaySize = 1, delayPos = 0;
    float fbL = 0.f, fbR = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaLaVieGuitareProcessor)
};
