#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <ProcessInterface/ExpandMode.hpp>
#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ConstraintModel;
class ScenarioModel;

/*
 * Command for vertical move so it does'nt have to resize anything on time axis
 * */

namespace Scenario
{
    namespace Command
    {
        class MoveConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("MoveConstraint", "MoveConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveConstraint();
                ~MoveConstraint();
                MoveConstraint(
                        ModelPath<ScenarioModel>&& scenarioPath,
                    const id_type<ConstraintModel>& id,
                    const TimeValue& date,
                    double y);

                void update(const ModelPath<ScenarioModel>&,
                            const id_type<ConstraintModel>& ,
                            const TimeValue& date,
                            double height);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ScenarioModel> m_path;
                id_type<ConstraintModel> m_constraint;
                double m_oldHeight{},
                       m_newHeight{};
        };
    }
}
