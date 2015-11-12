#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QJsonObject>
#include <tests/helpers/ForwardDeclaration.hpp>
#include <Process/ExpandMode.hpp>

class Process;
class RackModel;
class ConstraintModel;
namespace Scenario
{
    namespace Command
    {
    // TODO Rename file
        class InsertContentInConstraint final : public iscore::SerializableCommand
        {
                ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), InsertContentInConstraint, "InsertContentInConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                InsertContentInConstraint(
                    QJsonObject&& sourceConstraint,
                    Path<ConstraintModel>&&  targetConstraint,
                    ExpandMode mode);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                QJsonObject m_source;
                Path<ConstraintModel> m_target;
                ExpandMode m_mode{ExpandMode::Grow};

                QMap<Id<RackModel>, Id<RackModel>> m_rackIds;
                QMap<Id<Process>, Id<Process>> m_processIds;
        };
    }
}
