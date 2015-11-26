#pragma once
#include <QObject>
#include <functional>
#include <RtAudio.h>
#include <QDebug>
#include <mutex>
#include <libwatermark/Parameters.h>
#include <Audio/AudioBlock.hpp>


int generate(void *outputBuffer,
             void *inputBuffer,
             unsigned int nFrames,
             double streamTime,
             RtAudioStreamStatus status,
             void *userData);

template<typename data_type>
class RtAudioOutput
{
public:
        Parameters<data_type>& conf;
        using handle_t = std::function<int(void*)> ;
    RtAudioOutput(Parameters<data_type>& cfg):
        conf(cfg)
    {
        for(int i = 0; i < audio.getDeviceCount() ; i ++ )
        {

            auto info = audio.getDeviceInfo(3);
            qDebug() << "=====" << info.outputChannels << info.name.c_str() << "\n";
        }
        parameters.deviceId = 3;//audio.getDefaultOutputDevice();
        parameters.nChannels = 2;
        parameters.firstChannel = 0;
    }

    ~RtAudioOutput()
    {
        stopStream();
    }

    void startStream(handle_t handle)
    {
        using namespace std;
        using namespace placeholders;
        _handler = handle;

        if(!audio.isStreamOpen())
        {
            try {
                unsigned int sampleRate = this->conf.samplingRate;
                unsigned int bufferFrames = this->conf.bufferSize;
                audio.openStream( &parameters,
                                  nullptr,
                                  RTAUDIO_FLOAT32,
                                  sampleRate,
                                  &bufferFrames,
                                  &generate,
                                  (void*)this);

                audio.startStream();


            }
            catch (RtAudioError &error) {
                error.printMessage();
                stopStream();
                return;
            }
        }

        isRunning = true;

    }

    void stopStream()
    {
        try {
            qDebug() << Q_FUNC_INFO;
            // Stop and close the stream
            if(audio.isStreamRunning())
                audio.stopStream();

            if (audio.isStreamOpen())
                audio.closeStream();
            isRunning = false;
        }
        catch (RtAudioError &error) {
            error.printMessage();
        }
    }

    handle_t _handler;
private:


    RtAudio audio{RtAudio::LINUX_PULSE};
    RtAudio::StreamParameters parameters;
    bool isRunning{};
};



inline int generate(void *outputBuffer,
             void *inputBuffer,
             unsigned int nFrames,
             double streamTime,
             RtAudioStreamStatus status,
             void *userData)
{
    auto obj = static_cast<RtAudioOutput<float>*>(userData);
    if(nFrames < obj->conf.bufferSize)
    {
        std::cerr << "[WARNING] Buffer underrun.";
    }

    return obj->_handler(outputBuffer);
}

class AudioEngine : public QObject
{
        Q_OBJECT
    public:

        Parameters<float> params;
        void play();
        void stop();

    public slots:
        void addHandle(AudioBlock* block);
        void removeHandle(AudioBlock* block);

    private:
        std::mutex handles_lock;
        std::vector<AudioBlock*> handles;

        RtAudioOutput<float> output{params};
};

