#include "PluginProcessor.h"
#include <cmath>
#include <cstring>

namespace
{
constexpr double OVERFLOW_D = 2147483648.0;

std::vector<uint8_t> serializeParams(juce::AudioProcessorValueTreeState& apvts)
{
    std::vector<uint8_t> out;
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml != nullptr)
    {
        const auto text = xml->toString();
        out.assign(text.toRawUTF8(), text.toRawUTF8() + text.getNumBytesAsUTF8());
    }
    return out;
}

bool deserializeParams(juce::AudioProcessorValueTreeState& apvts,
                       const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0) return false;
    const auto text = juce::String::fromUTF8(reinterpret_cast<const char*>(data),
                                             static_cast<int>(len));
    auto xml = juce::parseXML(text);
    if (xml == nullptr) return false;
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    return true;
}
} // namespace

float AlphaFastDistEffect::knob(int value)
{
    return alpha::dsp::curve(static_cast<float>(value) * 0.0078125f);
}

void AlphaFastDistEffect::gains(float pre, float mix, float post, float& wet, float& dry)
{
    wet = mix * post;
    dry = (post - wet) * pre * post;
}

double AlphaFastDistEffect::level_at(int i, int step, int kind)
{
    if (i == 0) return 0.0;
    const double u = static_cast<double>(i) / 8191.0;
    const double db = 20.0 * std::log10(u);
    const double st = static_cast<double>(step);
    double shaped;
    if (kind == kTypeA)
    {
        const double k = (db >= -25.0) ? (db + 25.0) * (st + 1.0) * 0.1 - 25.0 : db;
        shaped = k + (9.0 - st) * 1.6;
    }
    else
    {
        shaped = -std::pow(-db, st * 0.02 + 0.8);
    }
    return static_cast<double>(alpha::dsp::round_ties_even(
        std::pow(10.0, shaped / 20.0) * 32767.0));
}

std::pair<int,int> AlphaFastDistEffect::curve_of(int threshold, int kind)
{
    long long n = static_cast<long long>(kind) * 10
                + static_cast<long long>(threshold) - 1;
    if (n < 0) n = 0;
    if (n > 19) n = 19;
    return { static_cast<int>(n % 10 + 1), static_cast<int>(n / 10) };
}

void AlphaFastDistEffect::make_table(int threshold, int kind, float wet,
                                     std::vector<double>& out)
{
    const auto co = curve_of(threshold, kind);
    const int th = co.first;
    const int kd = co.second;
    const int step = 10 - th;
    out.assign(static_cast<size_t>(kTable), 0.0);
    for (int i = 0; i < kTable; ++i)
        out[static_cast<size_t>(i)] = level_at(i, step, kd) / 32768.0
                                    * static_cast<double>(wet);
    out.push_back(out.back());
}

float AlphaFastDistEffect::chop(double y)
{
    const float f = static_cast<float>(y);
    if (std::fabs(static_cast<double>(f)) > std::fabs(y))
    {
        uint32_t bits;
        std::memcpy(&bits, &f, 4);
        bits -= 1;
        float out;
        std::memcpy(&out, &bits, 4);
        return out;
    }
    return f;
}

void AlphaFastDistEffect::derive()
{
    const float pre = knob(applied_[PRE]);
    float wet, dry;
    gains(pre, knob(applied_[MIX]), knob(applied_[POST]), wet, dry);
    make_table(applied_[THRESHOLD], applied_[TYPE], wet, table_);
    scale_ = static_cast<double>(pre) * 8191.0;
    dry_ = dry;
}

void AlphaFastDistEffect::setSettings(const std::array<int, NUM>& v)
{
    if (v == applied_) return;
    applied_ = v;
    derive();
}

float AlphaFastDistEffect::sample(float x) const
{
    const double x64 = static_cast<double>(x);
    const double p = std::fabs(x64 * scale_);
    double shaped;
    if (p < OVERFLOW_D)
    {
        const size_t i = static_cast<size_t>(std::min(p, 8191.0));
        const double f = p - std::trunc(p);
        const double a = table_[i];
        const double b = table_[i + 1];
        shaped = std::copysign(a + (b - a) * f, x64);
    }
    else
    {
        shaped = 0.0;
    }
    return chop(x64 * static_cast<double>(dry_) + shaped);
}

void AlphaFastDistEffect::process(float* l, float* r, int n)
{
    for (int i = 0; i < n; ++i)
    {
        l[i] = sample(l[i]);
        r[i] = sample(r[i]);
    }
}

void AlphaFastDistEffect::getState(std::vector<uint8_t>& out) const
{
    out.clear();
    for (int v : applied_)
        for (int b = 0; b < 4; ++b)
            out.push_back(static_cast<uint8_t>(v >> (8 * b)));
}

bool AlphaFastDistEffect::setState(const uint8_t* d, size_t n)
{
    if (n < static_cast<size_t>(kStateLen)) return false;
    std::array<int, NUM> v{};
    for (int i = 0; i < NUM; ++i)
    {
        const size_t at = 4 * static_cast<size_t>(i);
        v[static_cast<size_t>(i)] = static_cast<int32_t>(
            static_cast<uint32_t>(d[at])
            | (static_cast<uint32_t>(d[at+1]) << 8)
            | (static_cast<uint32_t>(d[at+2]) << 16)
            | (static_cast<uint32_t>(d[at+3]) << 24));
    }
    applied_ = v;
    derive();
    return true;
}

AlphaFastDistProcessor::AlphaFastDistProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", makeLayout()),
      presets("Alpha Fast Dist", 2, 1)
{
    preParam       = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("pre"));
    thresholdParam = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("threshold"));
    typeParam      = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("type"));
    mixParam       = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("mix"));
    postParam      = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("post"));

    rawPre       = apvts.getRawParameterValue("pre");
    rawThreshold = apvts.getRawParameterValue("threshold");
    rawType      = apvts.getRawParameterValue("type");
    rawMix       = apvts.getRawParameterValue("mix");
    rawPost      = apvts.getRawParameterValue("post");

    std::vector<AlphaPresets::FactoryPreset> f;
    f.push_back({ "Init", {} });
    presets.setFactoryPresets(std::move(f));

    pushParametersToDsp();
    setLatencySamples(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout
AlphaFastDistProcessor::makeLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterInt>("pre",       "Pre",       64, 192, 128));
    params.push_back(std::make_unique<AudioParameterInt>("threshold", "Threshold",  1,  10,  10));
    params.push_back(std::make_unique<AudioParameterInt>("type",      "Type",       0,   1,   0));
    params.push_back(std::make_unique<AudioParameterInt>("mix",       "Mix",        0, 128, 128));
    params.push_back(std::make_unique<AudioParameterInt>("post",      "Post",       0, 128, 128));
    return { params.begin(), params.end() };
}

void AlphaFastDistProcessor::seedDefaults()
{
    auto set = [](juce::AudioParameterInt* p, int def) {
        if (p != nullptr)
            p->setValueNotifyingHost(
                p->convertTo0to1(static_cast<float>(def)));
    };
    set(preParam,       AlphaFastDistEffect::kDefaults[AlphaFastDistEffect::PRE]);
    set(thresholdParam, AlphaFastDistEffect::kDefaults[AlphaFastDistEffect::THRESHOLD]);
    set(typeParam,      AlphaFastDistEffect::kDefaults[AlphaFastDistEffect::TYPE]);
    set(mixParam,       AlphaFastDistEffect::kDefaults[AlphaFastDistEffect::MIX]);
    set(postParam,      AlphaFastDistEffect::kDefaults[AlphaFastDistEffect::POST]);
}

void AlphaFastDistProcessor::pushParametersToDsp()
{
    const int pr = (rawPre       != nullptr) ? juce::roundToInt(rawPre->load())       : lastPre.load();
    const int th = (rawThreshold != nullptr) ? juce::roundToInt(rawThreshold->load()) : lastThreshold.load();
    const int ty = (rawType      != nullptr) ? juce::roundToInt(rawType->load())      : lastType.load();
    const int mx = (rawMix       != nullptr) ? juce::roundToInt(rawMix->load())       : lastMix.load();
    const int po = (rawPost      != nullptr) ? juce::roundToInt(rawPost->load())      : lastPost.load();

    if (pr == lastPre.load()
     && th == lastThreshold.load()
     && ty == lastType.load()
     && mx == lastMix.load()
     && po == lastPost.load())
        return;

    lastPre.store(pr);
    lastThreshold.store(th);
    lastType.store(ty);
    lastMix.store(mx);
    lastPost.store(po);

    effect.setSettings({ pr, th, ty, mx, po });
}

void AlphaFastDistProcessor::prepareToPlay(double, int) { effect.reset(); }
void AlphaFastDistProcessor::releaseResources() { effect.reset(); }

bool AlphaFastDistProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono()
                      || in == juce::AudioChannelSet::stereo());
}

void AlphaFastDistProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    pushParametersToDsp();

    const int n  = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();
    if (n == 0 || ch == 0) return;
    if (ch >= 2)
        effect.process(buffer.getWritePointer(0), buffer.getWritePointer(1), n);
    else
        for (int i = 0; i < n; ++i)
            buffer.setSample(0, i, effect.sample(buffer.getSample(0, i)));
}

int AlphaFastDistProcessor::getNumPrograms() { return presets.getNumPrograms(); }
int AlphaFastDistProcessor::getCurrentProgram() { return currentProgram; }

void AlphaFastDistProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= presets.getNumPrograms()) return;
    currentProgram = index;
    const int fc = static_cast<int>(presets.factoryPresets().size());
    if (index < fc)
    {
        const auto& fp = presets.factoryPresets()[static_cast<size_t>(index)];
        if (! deserializeParams(apvts, fp.payload.data(), fp.payload.size()))
            seedDefaults();
        pushParametersToDsp();
        return;
    }
    std::vector<uint8_t> payload;
    juce::String err;
    if (presets.loadUserPreset(index - fc, payload, err))
    {
        if (deserializeParams(apvts, payload.data(), payload.size()))
            pushParametersToDsp();
    }
}

const juce::String AlphaFastDistProcessor::getProgramName(int index)
{
    const auto names = presets.allProgramNames();
    if (index < 0 || index >= names.size()) return {};
    return names[index];
}

bool AlphaFastDistProcessor::saveCurrentAsUserPreset(const juce::String& name,
                                                     juce::String& err)
{
    return presets.saveUserPreset(name, serializeParams(apvts), err);
}

void AlphaFastDistProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto payload = serializeParams(apvts);
    destData.replaceAll(payload.data(), payload.size());
}

void AlphaFastDistProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (deserializeParams(apvts, static_cast<const uint8_t*>(data),
                          static_cast<size_t>(sizeInBytes)))
        pushParametersToDsp();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AlphaFastDistProcessor();
}