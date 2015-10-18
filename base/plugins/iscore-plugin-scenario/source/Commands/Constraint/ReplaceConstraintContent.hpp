#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QJsonObject>
#include <tests/helpers/ForwardDeclaration.hpp>
#include "ProcessInterface/ExpandMode.hpp"

class Process;
class RackModel;
class ConstraintModel;
namespace Scenario
{
    namespace Command
    {
        class ReplaceConstraintContent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("ScenarioControl", "ReplaceConstraintContent", "ReplaceConstraintContent")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ReplaceConstraintContent)
                ReplaceConstraintContent(
                    QJsonObject&& sourceConstraint,
                    Path<ConstraintModel>&&  targetConstraint,
                    ExpandMode mode);

                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                QJsonObject m_source;
                Path<ConstraintModel> m_target;
                ExpandMode m_mode{ExpandMode::Grow};

                QMap<Id<RackModel>, Id<RackModel>> m_rackIds;
                QMap<Id<Process>, Id<Process>> m_processIds;
        };
    }
}
