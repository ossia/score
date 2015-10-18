#pragma once
#include <iscore/command/AggregateCommand.hpp>

class Record : public iscore::AggregateCommand
{
         ISCORE_AGGREGATE_COMMAND_DECL("IScoreCohesionControl", Record, "Record")
    public:
        void undo() const override
        {
            m_cmds[1]->undo();
            m_cmds[0]->undo();
        }

};
