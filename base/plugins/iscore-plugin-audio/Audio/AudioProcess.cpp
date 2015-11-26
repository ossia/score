#include "AudioProcess.hpp"
Audio::Process::Process(AudioEngine& conf):
    m_conf{conf},
    m_start{OSSIA::State::create()},
    m_end{OSSIA::State::create()}
{

}


void Audio::Process::setAudioBlock(std::unique_ptr<AudioBlock>&& val)
{
    m_block = std::move(val);
}

std::shared_ptr<OSSIA::StateElement> Audio::Process::state(const OSSIA::TimeValue& t, const OSSIA::TimeValue&)
{
    if(double(t) == 0)
        if(m_block)
            m_block->start();

    return {};
}

const std::shared_ptr<OSSIA::State>&Audio::Process::getStartState() const
{
    return m_start;
}

const std::shared_ptr<OSSIA::State>&Audio::Process::getEndState() const
{
    return m_end;
}
