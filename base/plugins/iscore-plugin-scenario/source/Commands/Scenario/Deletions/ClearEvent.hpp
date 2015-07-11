#pragma once
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
        class ClearState : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("ClearState", "ClearState")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ClearState, "ScenarioControl")
                ClearState(ObjectPath&& path);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                QByteArray m_serializedStates;
        };
    }
}
