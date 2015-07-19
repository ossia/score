#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include "ProcessInterface/ExpandMode.hpp"

class ProcessModel;
class RackModel;
namespace Scenario
{
    namespace Command
    {
        class ReplaceConstraintContent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("ReplaceConstraintContent", "ReplaceConstraintContent")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ReplaceConstraintContent, "ScenarioControl")
                ReplaceConstraintContent(QJsonObject&& sourceConstraint,
                                      ObjectPath&&  targetConstraint,
                                      ExpandMode mode);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                QJsonObject m_source;
                ObjectPath m_target;
                ExpandMode m_mode{ExpandMode::Grow};

                QMap<id_type<RackModel>, id_type<RackModel>> m_rackIds;
                QMap<id_type<ProcessModel>, id_type<ProcessModel>> m_processIds;
        };
    }
}
