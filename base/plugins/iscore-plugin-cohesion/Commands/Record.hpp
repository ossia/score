#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Commands/IScoreCohesionCommandFactory.hpp>

class Record final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(IScoreCohesionCommandFactoryName(), Record, "Record")
        public:
            void undo() const override
        {
            // Undo 1
            auto it = m_cmds.begin();
            std::advance(it, 1);
            (*it)->undo();

            // Undo 0
            (*m_cmds.begin())->undo();
        }

};
