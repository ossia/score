#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "Commands/Scenario/MoveEvent.hpp"
#include <iscore/tools/ObjectPath.hpp>

#include <QPointF>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>

namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The ResizeConstraintCommand class
        *
        * This command creates an Event, which is linked to the first event in the
        * scenario.
        */
        class ResizeBaseConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(ResizeBaseConstraint, "ScenarioControl")
                ResizeBaseConstraint(ObjectPath&& constraintPath,
                                     TimeValue duration);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                TimeValue m_oldDuration {};
                TimeValue m_newDuration {};
        };
    }
}
