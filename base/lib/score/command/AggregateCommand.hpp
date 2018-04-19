#pragma once
#include <algorithm>
#include <list>
#include <memory>
#include <score/command/Command.hpp>

namespace score
{
/**
 * @brief Allows for grouping of multiple commands in a single one.
 *
 * Useful for macros, meta-commands, etc.
 */
class SCORE_LIB_BASE_EXPORT AggregateCommand : public score::Command
{
  using command_ptr = score::Command*;

public:
  AggregateCommand() = default;
  virtual ~AggregateCommand();

  template <typename T>
  AggregateCommand(T* cmd) : AggregateCommand{}
  {
    m_cmds.push_front(cmd);
  }

  /**
   * This constructor allows to pass a list of commands in argument.
   *
   * e.g. new AggregateCommand{new MyCommand, new MySecondCommand, new
   * MyThirdCommand};
   */
  template <typename T, typename... Args>
  AggregateCommand(T* cmd, Args&&... remaining)
      : AggregateCommand{std::forward<Args>(remaining)...}
  {
    m_cmds.push_front(cmd);
  }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  //! Add a command to be redone after the others.
  void addCommand(score::Command* cmd);

  //! Number of commands in this aggregate
  int count() const;

  const auto& commands() const
  {
    return m_cmds;
  }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

  std::list<score::Command*> m_cmds;
};
}
