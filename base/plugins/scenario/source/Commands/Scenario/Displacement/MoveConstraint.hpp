#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <ProcessInterface/ExpandMode.hpp>
class ConstraintModel;

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>
namespace Scenario
{
    namespace Command
    {
        class MoveEvent;
        class MoveConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveConstraint();
                ~MoveConstraint();
                MoveConstraint(
                    ObjectPath&& scenarioPath,
                    const id_type<ConstraintModel>& id,
                    const TimeValue& date,
                    double y,
                    ExpandMode mode,
                    bool changeDate = true);

                void update(const ObjectPath&,
                            const id_type<ConstraintModel>& ,
                            const TimeValue& date,
                            double height,
                            ExpandMode);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                MoveEvent* m_cmd{};
                ObjectPath m_path;
                id_type<ConstraintModel> m_constraint;
                double m_oldHeightPosition{},
                       m_newHeightPosition{},
                       m_eventHeight{};
                bool m_changeDate{};
        };
    }
}
