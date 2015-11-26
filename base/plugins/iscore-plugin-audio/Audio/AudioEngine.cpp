#include "AudioEngine.hpp"
#include <iscore/tools/std/StdlibWrapper.hpp>

void AudioEngine::play()
{
    output.startStream([&] (void* outputBuffer) {
        float *out = static_cast<float *>(outputBuffer);
        for(int i = 0; i < params.bufferSize; i++)
            out[i] = 0;

        bool turn = true;
        int handle_i = 0;
        while(turn)
        {
            std::lock_guard<std::mutex> lock(handles_lock);
            if(handle_i < handles.size())
            {
                AudioBlock* proc = handles[handle_i];
                auto data = proc->data(params.bufferSize, proc->currentBuffer++, 0);

                if(data.size() >= params.bufferSize)
                {
                    for(int i = 0; i < params.bufferSize; i++)
                    {
                        out[i] += data[i] * 0.1;
                    }
                }

                handle_i++;
            }
            else
            {
                turn = false;
            }
        }

        return 0;
    });
}

void AudioEngine::stop()
{
    output.stopStream();

}

void AudioEngine::addHandle(AudioBlock* block)
{
    std::lock_guard<std::mutex> lock(handles_lock);

    auto it = find(handles, block);
    if(it == handles.end())
        handles.push_back(block);
}

void AudioEngine::removeHandle(AudioBlock* block)
{
    std::lock_guard<std::mutex> lock(handles_lock);
    auto it = find(handles, block);
    if(it != handles.end())
    {
        handles.erase(it);
    }
}
