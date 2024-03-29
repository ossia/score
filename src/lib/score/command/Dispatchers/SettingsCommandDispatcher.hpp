#pragma once
#include <score/command/SettingsCommand.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <memory>
namespace score
{
class SettingsCommandDispatcher
{
public:
  template <typename TheCommand, typename... Args>
  void submit(Args&&... args)
  {
    static_assert(!TheCommand::is_deferred, "Don't use a deferred command");
    auto it = commands.find(TheCommand::static_key());
    if(it != commands.end())
    {
      static_cast<TheCommand&>(*it->second).update(std::forward<Args>(args)...);
      it->second->redo();
    }
    else
    {
      auto cmd = std::make_unique<TheCommand>(std::forward<Args>(args)...);
      cmd->redo();
      commands.insert(std::make_pair(TheCommand::static_key(), std::move(cmd)));
    }
  }

  template <typename TheCommand, typename... Args>
  void submitDeferredCommand(Args&&... args)
  {
    static_assert(TheCommand::is_deferred, "Use a deferred command");
    auto it = deferred.find(TheCommand::static_key());
    if(it != deferred.end())
    {
      static_cast<TheCommand&>(*it->second).update(std::forward<Args>(args)...);
    }
    else
    {
      auto cmd = std::make_unique<TheCommand>(std::forward<Args>(args)...);
      deferred.insert(std::make_pair(TheCommand::static_key(), std::move(cmd)));
    }
  }

  void commit()
  {
    commands.clear();
    for(auto& it : deferred)
    {
      it.second->redo();
    }
    deferred.clear();
  }

  void rollback()
  {
    for(auto& it : commands)
    {
      it.second->undo();
    }
    commands.clear();
    deferred.clear();
  }

private:
  score::hash_map<CommandKey, std::unique_ptr<score::SettingsCommandBase>> commands;
  score::hash_map<CommandKey, std::unique_ptr<score::SettingsCommandBase>> deferred;
};
}
