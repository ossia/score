#pragma once
#include <Recording/Commands/RecordingCommandFactory.hpp>
#include <iscore/command/AggregateCommand.hpp>

namespace Recording
{
class Record final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(RecordingCommandFactoryName(), Record, "Record")
public:
  void undo() const override
  {
    const int N = m_cmds.size();
    // Undo 1
    if(N >= 2)
    {
      auto it = m_cmds.begin();
      std::advance(it, 1);
      (*it)->undo();
    }

    if(N >= 1)
    {
      // Undo 0
      (*m_cmds.begin())->undo();
    }
  }
};
}
