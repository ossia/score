#pragma once
#include <algorithm>
#include <iscore/command/Command.hpp>
#include <list>
#include <memory>

namespace iscore
{
/**
* @brief Allows for grouping of multiple commands in a single one.
*
* Useful for macros, meta-commands, etc.
*/
class ISCORE_LIB_BASE_EXPORT AggregateCommand
    : public iscore::Command
{
        using command_ptr = std::unique_ptr<iscore::Command>;
public:
  AggregateCommand() = default;
  virtual ~AggregateCommand();

  template <typename T>
  AggregateCommand(T* cmd) : AggregateCommand{}
  {
      m_cmds.push_front(command_ptr{cmd});
  }

  /**
   * This constructor allows to pass a list of commands in argument.
   *
   * e.g. new AggregateCommand{new MyCommand, new MySecondCommand, new MyThirdCommand};
   */
  template <typename T, typename... Args>
  AggregateCommand(T* cmd, Args&&... remaining)
      : AggregateCommand{std::forward<Args>(remaining)...}
  {
    m_cmds.push_front(command_ptr{cmd});
  }

  void undo() const override;
  void redo() const override;

  void addCommand(iscore::Command* cmd);

  int count() const;

  const auto& commands() const
  {
    return m_cmds;
  }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

  std::list<std::unique_ptr<iscore::Command>> m_cmds;
};
}
