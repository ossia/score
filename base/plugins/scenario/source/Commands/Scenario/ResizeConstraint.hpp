#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include "Commands/Scenario/MoveEvent.hpp"
#include <tools/ObjectPath.hpp>

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
        class ResizeConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ResizeConstraint();
                ~ResizeConstraint();
                ResizeConstraint(ObjectPath&& constraintPath, TimeValue duration);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                MoveEvent* m_cmd {};
                TimeValue m_oldEndDate {};
        };
    }
}
