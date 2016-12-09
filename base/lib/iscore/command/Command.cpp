#include "Command.hpp"

#include <iscore/application/ApplicationContext.hpp>
iscore::Command::Command() :
    context{iscore::AppContext()},
    m_timestamp{
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())}
{
}

iscore::Command::~Command() = default;

quint32 iscore::Command::timestamp() const
{
  return static_cast<quint32>(m_timestamp.count());
}

void iscore::Command::setTimestamp(quint32 stmp)
{
  m_timestamp = std::chrono::duration<quint32>(stmp);
}
