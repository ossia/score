#pragma once
#include <vector>
#include <faust/dsp/llvm-dsp.h>
#include <QString>
#include <libwatermark/Parameters.h>

class AudioEngine;
class AudioBlock
{
    public:
        AudioBlock(AudioEngine& en):
            m_engine{en}
        {

        }

        const Parameters<float>& parameters() const;

        virtual ~AudioBlock();
        virtual std::vector<float> data(int size, int buffer, int offset) const = 0;

        void start();

        int currentBuffer = 0;
        int offset = 0; // Offset in samples between the playing audio and the buffer.

        AudioEngine& m_engine;
};

class FaustAudioBlock : public AudioBlock
{
    public:
        llvm_dsp_factory* m_faustFactory{};
        llvm_dsp* m_faustPlug{};
        std::vector<float> m_audio;


        FaustAudioBlock(
                const QString& script,
                std::vector<float> audio,
                AudioEngine& params);

        ~FaustAudioBlock();


        std::vector<float> data(int size, int buffer, int offset) const override;


};
