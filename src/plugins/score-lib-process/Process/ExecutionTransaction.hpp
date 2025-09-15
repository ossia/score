#pragma once
#include <Process/ExecutionCommand.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/detail/thread.hpp>

#include <verdigris>

namespace Execution
{
struct Context;
template <typename... T>
inline constexpr auto gc(T&&... t) noexcept
{
  return
      [... gced = std::move(t)] { OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui); };
}

struct Transaction
{
  const Context& context;
  std::vector<ExecutionCommand> commands;
  explicit Transaction(const Context& ctx) noexcept
      : context{ctx}
  {
  }

  Transaction(Transaction&& other) noexcept
      : context{other.context}
      , commands(std::move(other.commands))
  {
  }

  Transaction& operator=(Transaction&& other) noexcept
  {
    commands = std::move(other.commands);
    return *this;
  }
  void reserve(std::size_t sz) noexcept { commands.reserve(sz); }
  bool empty() const noexcept { return commands.empty(); }
  template <typename T>
  void push_back(T&& t) noexcept
  {
    commands.push_back(std::move(t));
  }

  void run_all()
  {
    context.executionQueue.enqueue(
        [t = std::move(*this)]() mutable { t.run_all_in_exec(); });
  }

  void run_all_in_exec()
  {
    for(auto& cmd : commands)
      cmd();
  }
};

}
Q_DECLARE_METATYPE(Execution::Transaction*)
W_REGISTER_ARGTYPE(Execution::Transaction*)
