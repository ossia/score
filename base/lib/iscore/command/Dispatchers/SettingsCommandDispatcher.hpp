#pragma once
#include <map>
#include <memory>
#include <iscore/command/SettingsCommand.hpp>
namespace iscore
{
class SettingsCommandDispatcher
{
    public:
        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
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
               commands.insert(
                           std::make_pair(
                               TheCommand::static_key(),
                               std::move(cmd)));

            }
        }

        void commit()
        {
            commands.clear();
        }

        void rollback()
        {
            for(auto& it : commands)
            {
                it.second->undo();
            }
            commands.clear();
        }

    private:
        std::map<CommandFactoryKey, std::unique_ptr<iscore::Command>> commands;
};
}
