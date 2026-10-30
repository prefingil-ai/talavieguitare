#include "PluginProcessor.h"

//==============================  VOIX  ==============================
void GuitarVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    const double sr   = getSampleRate();
    const float  sus  = apvts.getRawParameterValue ("sustain")->load();      // secondes (-60 dB)
    const bool   soft = apvts.getRawParameterValue ("voicing")->load() > 0.5f;   // guitare douce ?
    const float  bri  = apvts.getRawParameterValue ("brightness")->load() * (soft ? 0.55f : 1.f);
    const float  rel  = apvts.getRawParameterValue ("release")->load();      // secondes
    gain = soft ? 0.32f : 0.4f;

    const double freq = juce::MidiMessage::getMidiNoteInHertz (midiNote);

    // Filtre de boucle (passe-bas à 1 pôle) : plus 'a' est petit, plus c'est brillant
    a = juce::jlimit (0.02f, 0.7f, 0.58f - 0.5f * bri - 0.05f * velocity + (soft ? 0.06f : 0.f));   // corde acier : plus brillante
    const double lpDelay = a / (1.0 - a);

    // Longueur de boucle = entier + fraction (filtre passe-tout pour l'accord fin)
    const double target = sr / freq - lpDelay;
    delayLen = juce::jmax (2, (int) (target - 0.5));
    const double d = target - delayLen;
    eta = (float) ((1.0 - d) / (1.0 + d));

    // Décroissance : -60 dB en 'sus' secondes
    g = (float) juce::jmin (0.9995, std::pow (0.001, (delayLen + d + lpDelay) / (sr * sus)));

    // Excitation : coup de médiator (un seul lissage = attaque plus nette que le nylon)
    buffer.assign ((size_t) delayLen, 0.f);
    std::vector<float> exc ((size_t) delayLen);
    for (auto& s : exc) s = rng.nextFloat() * 2.f - 1.f;
    // Médiator (1 lissage, net) ou doigts (3 lissages, très doux)
    for (int pass = 0; pass < (soft ? 3 : 1); ++pass)
    {
        const float c = soft ? 0.5f : 0.6f;
        float prev = 0.f;
        for (auto& s : exc) { prev = c * s + (1.f - c) * prev; s = prev; }
    }
    tapWeight    = soft ? 0.45f : 0.7f;
    pickupOffset = juce::jmax (1, (int) ((soft ? 0.27 : 0.17) * delayLen));   // micro côté manche = plus chaud
    const int k  = juce::jmax (1, (int) ((soft ? 0.25 : 0.18) * delayLen));
    float mean = 0.f, peak = 1e-9f;
    for (int i = 0; i < delayLen; ++i)
    {
        buffer[(size_t) i] = exc[(size_t) i] - (i >= k ? exc[(size_t) (i - k)] : 0.f);
        mean += buffer[(size_t) i];
    }
    mean /= (float) delayLen;
    for (auto& s : buffer) { s -= mean; peak = juce::jmax (peak, std::abs (s)); }
    const float v = soft ? 0.25f + 0.45f * velocity : 0.35f + 0.65f * velocity;
    for (auto& s : buffer) s *= v / peak;

    pos = 0; x1 = y1 = lp = 0.f;
    level = 1.f; releasing = false; silentCount = 0;
    relCoef = (float) std::pow (0.001, 1.0 / (sr * rel));
}

void GuitarVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) releasing = true;
    else              clearCurrentNote();
}

void GuitarVoice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (! isVoiceActive()) return;

    for (int i = 0; i < num; ++i)
    {
        const float v  = buffer[(size_t) pos];
        const float ap = eta * v + x1 - eta * y1;
        x1 = v; y1 = ap;
        lp = (1.f - a) * ap + a * lp;
        buffer[(size_t) pos] = lp * g;
        if (++pos >= delayLen) pos = 0;

        if (releasing) level *= relCoef;
        const float tap = buffer[(size_t) ((pos + pickupOffset) % delayLen)];
        const float s = ((v - tapWeight * tap) / (1.f + 0.5f * tapWeight)) * level * gain;     // son capté par le micro

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            out.addSample (ch, start + i, s);

        silentCount = (std::abs (s) < 1.0e-5f) ? silentCount + 1 : 0;
        if (level < 0.001f || silentCount > (int) getSampleRate() / 5)
        {
            clearCurrentNote();
            break;
        }
    }
}

//============================  PROCESSEUR  ===========================
juce::AudioProcessorValueTreeState::ParameterLayout TaLaVieGuitareProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("voicing", "Guitare",
                 juce::StringArray { "Afrobeat electrique", "Douce chaleureuse" }, 0));
    p.push_back (std::make_unique<P> ("sustain",    "Sustain (s)",     R (0.5f, 15.f, 0.01f, 0.6f), 4.f));
    p.push_back (std::make_unique<P> ("release",    "Release (s)",     R (0.05f, 4.f, 0.01f, 0.6f), 0.4f));
    p.push_back (std::make_unique<P> ("brightness", "Brillance",       R (0.f, 1.f, 0.01f), 0.6f));
    p.push_back (std::make_unique<P> ("drive",      "Chaleur ampli",   R (0.f, 1.f, 0.01f), 0.4f));
    p.push_back (std::make_unique<P> ("chorus",     "Chorus",          R (0.f, 1.f, 0.01f), 0.4f));
    p.push_back (std::make_unique<P> ("delayMix",   "Delay mix",       R (0.f, 1.f, 0.01f), 0.25f));
    p.push_back (std::make_unique<P> ("delayTime",  "Delay temps (ms)",R (100.f, 900.f, 1.f), 450.f));
    p.push_back (std::make_unique<P> ("reverbSize", "Reverb taille",   R (0.f, 1.f, 0.01f), 0.6f));
    p.push_back (std::make_unique<P> ("reverbMix",  "Reverb mix",      R (0.f, 1.f, 0.01f), 0.3f));
    p.push_back (std::make_unique<P> ("volume",     "Volume",          R (0.f, 1.5f, 0.01f), 1.0f));
    return { p.begin(), p.end() };
}

TaLaVieGuitareProcessor::TaLaVieGuitareProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    synth.addSound (new GuitarSound());
    for (int i = 0; i < 12; ++i)
        synth.addVoice (new GuitarVoice (apvts));
}

void TaLaVieGuitareProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sr);
    reverb.setSampleRate (sr);
    reverb.reset();

    // Deux sons d'ampli : [0] afrobeat (présence 2,3 kHz) ; [1] douce (rondeur 260 Hz, coupe-haut 3,2 kHz)
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    hpC[0]  = Coefs::makeHighPass (sr, 90.f);
    midC[0] = Coefs::makePeakFilter (sr, 2300.f, 1.2f, 1.6f);
    lpC[0]  = Coefs::makeLowPass (sr, 6500.f);
    hpC[1]  = Coefs::makeHighPass (sr, 80.f);
    midC[1] = Coefs::makePeakFilter (sr, 260.f, 0.9f, 1.5f);
    lpC[1]  = Coefs::makeLowPass (sr, 3200.f);
    lastVoicing = -1;
    for (int ch = 0; ch < 2; ++ch) { hp[ch].reset(); mid[ch].reset(); lpf[ch].reset(); }

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    chorusFx.prepare (spec);
    chorusFx.setRate (0.8f);
    chorusFx.setDepth (0.25f);
    chorusFx.setCentreDelay (12.f);
    chorusFx.setFeedback (0.f);

    delaySize = (int) (sr * 1.2);
    for (auto& d : delayBuf) d.assign ((size_t) delaySize, 0.f);
    delayPos = 0; fbL = fbR = 0.f;
}

void TaLaVieGuitareProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buf.clear();
    synth.renderNextBlock (buf, midi, 0, buf.getNumSamples());

    const int n = buf.getNumSamples();
    auto val = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // 1) Ampli : EQ + saturation douce
    const int   voicing = (int) val ("voicing");
    const bool  soft    = voicing == 1;
    if (voicing != lastVoicing)
    {
        for (int ch = 0; ch < 2; ++ch)
        { hp[ch].coefficients = hpC[voicing]; mid[ch].coefficients = midC[voicing]; lpf[ch].coefficients = lpC[voicing]; }
        lastVoicing = voicing;
    }
    const float drive = val ("drive") * (soft ? 0.35f : 1.f);
    const float k = 1.f + 1.5f * drive;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float s = hp[ch].processSample (d[i]);
            s = s + 0.5f * (mid[ch].processSample (s) - s);
            s = std::tanh (k * s) / k;
            d[i] = lpf[ch].processSample (s);
        }
    }

    // 2) Chorus (mélangé au son direct)
    chorusFx.setMix (0.5f * val ("chorus") * (soft ? 0.6f : 1.f));
    {
        juce::dsp::AudioBlock<float> block (buf);
        chorusFx.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // 3) Delay ping-pong (par défaut une croche pointée à 100 BPM)
    const int   dTime = juce::jlimit (1, delaySize - 1, (int) (getSampleRate() * val ("delayTime") / 1000.0));
    const float dMix  = val ("delayMix");
    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const int rp = (delayPos - dTime + delaySize) % delaySize;
        float outL = delayBuf[0][(size_t) rp];
        float outR = delayBuf[1][(size_t) rp];
        fbL += 0.5f * (outL - fbL);               // feedback adouci
        fbR += 0.5f * (outR - fbR);
        delayBuf[0][(size_t) delayPos] = L[i] + 0.32f * fbR;
        delayBuf[1][(size_t) delayPos] = R[i] + 0.32f * fbL;
        L[i] += dMix * 0.6f * outL;
        R[i] += dMix * 0.6f * outR;
        if (++delayPos >= delaySize) delayPos = 0;
    }

    // 4) Reverb plate douce
    const float mix = val ("reverbMix");
    juce::Reverb::Parameters rp;
    rp.roomSize = val ("reverbSize");
    rp.damping  = soft ? 0.7f : 0.55f;
    rp.wetLevel = mix * (soft ? 0.65f : 0.5f);
    rp.dryLevel = 1.f - 0.3f * mix;
    rp.width    = 1.f;
    reverb.setParameters (rp);
    reverb.processStereo (L, R, n);

    buf.applyGain (val ("volume"));
}

juce::AudioProcessorEditor* TaLaVieGuitareProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);   // interface simple, on la personnalisera ensuite
}

void TaLaVieGuitareProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void TaLaVieGuitareProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TaLaVieGuitareProcessor(); }
