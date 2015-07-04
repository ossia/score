#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "CreateConstraint.hpp"

namespace Scenario
{
namespace Command
{

class ScenarioCreateConstraint_State_Event : public iscore::SerializableCommand
{
    public:
        ScenarioCreateConstraint_State_Event()
        {

        }

        void undo() override
        {

        }

        void redo() override
        {

        }

    protected:
        void serializeImpl(QDataStream&) const override
        {

        }

        void deserializeImpl(QDataStream&) override
        {

        }
};

class ScenarioCreateConstraint_State_Event_TimeNode : public iscore::SerializableCommand
{
    public:
        ScenarioCreateConstraint_State_Event_TimeNode()
        {

        }

        void undo() override
        {

        }

        void redo() override
        {

        }

    protected:
        void serializeImpl(QDataStream&) const override
        {

        }

        void deserializeImpl(QDataStream&) override
        {

        }
};

}}
