#pragma once
#include <iscore/command/AggregateCommand.hpp>

class Record : public iscore::AggregateCommand
{
         ISCORE_COMMAND_DECL("IScoreCohesionControl", "Record", "Record")
    public:
        Record():
            AggregateCommand{factoryName(),
                             commandName(),
                             description()}
        {

        }

        void undo() override
        {
            m_cmds[1]->undo();
            m_cmds[0]->undo();
        }

};
