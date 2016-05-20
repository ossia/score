#include "Command.hpp"

iscore::Command::Command():
    context{iscore::AppContext()}
{

}

iscore::Command::~Command() = default;

quint32 iscore::Command::timestamp() const
{
    return static_cast<quint32>(m_timestamp.count());
}

void iscore::Command::setTimestamp(quint32 stmp)
{
    m_timestamp = std::chrono::duration<quint32> (stmp);
}
