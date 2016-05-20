#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Scenario
{
namespace Command
{
template<class T>
class ChangeElementLabel final : public iscore::SerializableCommand
{
        // No ISCORE_COMMAND here since it's a template.
    public:
        const CommandParentFactoryKey& parentKey() const override
        {
            return ScenarioCommandFactoryName();
        }
        static const CommandFactoryKey& static_key()
        {
            auto name = QString("ChangeElementLabel_")  + Metadata<ObjectKey_k, T>::get();
            static const CommandFactoryKey kagi{std::move(name)};
            return kagi;
        }
        const CommandFactoryKey& key() const override
        {
            return static_key();
        }
        QString description() const override
        {
            return QObject::tr("Change %1 label").arg(Metadata<Description_k, T>::get());
        }

        ChangeElementLabel() = default;

        ChangeElementLabel(Path<T>&& path, QString newLabel) :
            m_path {std::move(path) },
            m_newLabel {std::move(newLabel)}
        {
            auto& obj = m_path.find();
            m_oldLabel = obj.metadata.label();
        }

        void undo() const override
        {
            auto& obj = m_path.find();
            obj.metadata.setLabel(m_oldLabel);
        }

        void redo() const override
        {
            auto& obj = m_path.find();
            obj.metadata.setLabel(m_newLabel);
        }

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_path << m_oldLabel << m_newLabel;
        }

        void deserializeImpl(DataStreamOutput& s) override
        {
            s >> m_path >> m_oldLabel >> m_newLabel;
        }

    private:
        Path<T> m_path;
        QString m_newLabel;
        QString m_oldLabel;
};
}
}

ISCORE_COMMAND_DECL_T(ChangeElementLabel<ConstraintModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<EventModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<TimeNodeModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<StateModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<Process::ProcessModel>)
