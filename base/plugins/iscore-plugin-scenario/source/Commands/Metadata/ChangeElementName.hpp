#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementName : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                static const char * factoryName()
                {
                    return ScenarioCommandFactoryName();
                }
                static const char * commandName()
                {
                    static QByteArray name = QString{"ChangeElementName_%1"}.arg(T::staticMetaObject.className()).toUtf8();
                    return name.constData();
                }
                static QString description()
                {
                    return QObject::tr("Change %1 name").arg(T::prettyName());
                }

                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ChangeElementName)
                ChangeElementName(Path<T>&& path, QString newName) :
                    SerializableCommand{factoryName(),
                                        commandName(),
                                        description()},
                    m_path {std::move(path) },
                    m_newName {newName}
                {
                    auto& obj = m_path.find();
                    m_oldName = obj.metadata.name();
                }

                void undo() const override
                {
                    auto& obj = m_path.find();
                    obj.metadata.setName(m_oldName);
                }

                void redo() const override
                {
                    auto& obj = m_path.find();
                    obj.metadata.setName(m_newName);
                }

            protected:
                virtual void serializeImpl(QDataStream& s) const override
                {
                    s << m_path << m_oldName << m_newName;
                }

                virtual void deserializeImpl(QDataStream& s) override
                {
                    s >> m_path >> m_oldName >> m_newName;
                }

            private:
                Path<T> m_path;
                QString m_newName;
                QString m_oldName;
        };
    }
}
