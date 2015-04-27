#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class ProcessSharedModelInterface;
class BoxModel;
namespace Scenario
{
    namespace Command
    {
        class CopyConstraintContent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(CopyConstraintContent, "ScenarioControl")
                CopyConstraintContent(QJsonObject&& sourceConstraint,
                                      ObjectPath&&  targetConstraint);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                QJsonObject m_source;
                ObjectPath m_target;

                QVector<id_type<BoxModel>> m_boxIds;
                QVector<id_type<ProcessSharedModelInterface>> m_processIds;
        };
    }
}
