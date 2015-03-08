#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

struct ConstraintData;
class ConstraintModel;

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>
namespace Scenario
{
    namespace Command
    {
        class MoveConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(MoveConstraint, "ScenarioControl")
                MoveConstraint(ObjectPath&& scenarioPath, ConstraintData d);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ConstraintModel> m_constraintId {};

                double m_oldHeightPosition {};
                double m_newHeightPosition {};
                TimeValue m_oldX {};
                TimeValue m_newX {};
        };
    }
}
