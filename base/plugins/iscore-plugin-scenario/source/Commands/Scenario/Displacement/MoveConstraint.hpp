#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <ProcessInterface/ExpandMode.hpp>
class ConstraintModel;

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>


/*
 * Command for vertical move so it does'nt have to resize anything on time axis
 * */

namespace Scenario
{
    namespace Command
    {
        class MoveConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveConstraint();
                ~MoveConstraint();
                MoveConstraint(ObjectPath&& scenarioPath,
                    const id_type<ConstraintModel>& id,
                    const TimeValue& date,
                    double y);

                void update(const ObjectPath&,
                            const id_type<ConstraintModel>& ,
                            const TimeValue& date,
                            double height);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ConstraintModel> m_constraint;
                double m_oldHeightPosition{},
                       m_newHeightPosition{},
                       m_eventHeight{};
        };
    }
}
