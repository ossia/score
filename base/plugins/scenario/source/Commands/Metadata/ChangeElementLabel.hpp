#pragma once
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementLabel : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                static const char * className()
                {
                    static QByteArray name = QString{"ChangeElementLabel_%1"}.arg(T::staticMetaObject.className()).toLatin1();
                    return name.constData();
                }
                static QString description()
                {
                    return QObject::tr("Change %1 label").arg(T::prettyName());
                }

                ISCORE_COMMAND_DEFAULT_CTOR(ChangeElementLabel, "ScenarioControl")
                ChangeElementLabel(ObjectPath&& path, QString newLabel) :
                    SerializableCommand{"ScenarioControl",
                                        className(),
                                        description()},
                    m_path {std::move(path) },
                    m_newLabel {newLabel}
                {
                    auto obj = m_path.find<T>();
                    m_oldLabel = obj->metadata.label();
                }

                virtual void undo() override
                {
                    auto obj = m_path.find<typename std::remove_const<T>::type>();
                    obj->metadata.setLabel(m_oldLabel);
                }

                virtual void redo() override
                {
                    auto obj = m_path.find<typename std::remove_const<T>::type>();
                    obj->metadata.setLabel(m_newLabel);
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
                ObjectPath m_path;
                QString m_newLabel;
                QString m_oldLabel;
        };
    }
}
