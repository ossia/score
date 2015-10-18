#pragma once
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementLabel : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                static const char * commandName()
                {
                    static QByteArray name = QString{"ChangeElementLabel_%1"}.arg(T::staticMetaObject.className()).toUtf8();
                    return name.constData();
                }
                static QString description()
                {
                    return QObject::tr("Change %1 label").arg(T::prettyName());
                }

                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(ChangeElementLabel, "ScenarioControl")
                ChangeElementLabel(Path<T>&& path, QString newLabel) :
                    SerializableCommand{"ScenarioControl",
                                        commandName(),
                                        description()},
                    m_path {std::move(path) },
                    m_newLabel {newLabel}
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
                virtual void serializeImpl(QDataStream& s) const override
                {
                    s << m_path << m_oldLabel << m_newLabel;
                }

                virtual void deserializeImpl(QDataStream& s) override
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
