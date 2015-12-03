#include "AudioBlock.hpp"
#include "AudioEngine.hpp"
#include <array>
const Parameters<float>&AudioBlock::parameters() const
{ return m_engine.params; }

AudioBlock::~AudioBlock()
{
    m_engine.removeHandle(this);

}

void AudioBlock::start()
{
    m_engine.addHandle(this);
}

void AudioBlock::stop()
{
    m_engine.removeHandle(this);
}

FaustAudioBlock::FaustAudioBlock(
        const QString& script,
        std::vector<float> audio,
        AudioEngine& params):
    AudioBlock{params},
    m_audio{audio}
{
    std::string target;
    m_faustFactory = createDSPFactoryFromString(
                         "name",
                         "phasor(f)   = f/48000 : (+,1.0:fmod) ~ _ ; process = phasor(440) * 6.28 : sin;",//script.toStdString(),
                         0, nullptr, "", target);
    if(m_faustFactory)
    {
        m_faustPlug = createDSPInstance(m_faustFactory);
    }
}

FaustAudioBlock::~FaustAudioBlock()
{
    deleteDSPFactory(m_faustFactory);
}

std::vector<float> FaustAudioBlock::data(int size, int buffer, int off) const
{
    if(m_faustPlug)
    {/*
        if(buffer * parameters().bufferSize > m_audio.size())
        {
            return {};
        }
        else
        {
        */
            float ** in_vec;
            in_vec = new float*[2];
            in_vec[0] = new float[parameters().bufferSize];
            in_vec[1] = new float[parameters().bufferSize];

            float ** out_vec;
            out_vec = new float*[2];
            out_vec[0] = new float[parameters().bufferSize];
            out_vec[1] = new float[parameters().bufferSize];
            /*for(int i = 0; i < size; i++)
            {
                vec[0][i] = m_audio[buffer * size + i];
            }*/

            m_faustPlug->compute(parameters().bufferSize, in_vec, out_vec);
            return std::vector<float>(out_vec[0], out_vec[0] + parameters().bufferSize);

          //  std::vector<float> out(vec[1], vec[1] + buffer);
          //  return out;
        //}
    }
    else
    {
        return {};

    }


}
