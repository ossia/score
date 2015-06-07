#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "Commands/Scenario/Displacement/MoveEvent.hpp"
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
        class ResizeConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ResizeConstraint();
                ~ResizeConstraint();
                ResizeConstraint(ObjectPath&& constraintPath,
                                 TimeValue duration,
                                 ExpandMode mode);
                virtual void undo() override;
                virtual void redo() override;

                void update(const ObjectPath& constraintPath,
                            const TimeValue& duration,
                            ExpandMode mode);

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                MoveEvent* m_cmd {};

                id_type<EventModel> m_endEvent;
                TimeValue m_constraintStartDate;
                double m_endEventHeightPercentage{};
        };
    }
}
