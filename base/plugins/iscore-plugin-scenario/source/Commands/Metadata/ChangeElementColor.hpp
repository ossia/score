#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementColor : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                static constexpr const char * factoryName()
                {
                    return ScenarioCommandFactoryName();
                }
                static const char * commandName()
                {
                    static QByteArray name = QString{"ChangeElementColor_%1"}.arg(T::staticMetaObject.className()).toUtf8();
                    return name.constData();
                }
                static QString description()
                {
                    return QObject::tr("Change %1 color").arg(T::prettyName());
                }

                ChangeElementColor():
                    SerializableCommand {factoryName(),
                                         commandName(),
                                         description()}
                { }

                ChangeElementColor(Path<T>&& path, QColor newLabel) :
                    SerializableCommand {factoryName(),
                                         commandName(),
                                         description()},
                    m_path {std::move(path) },
                    m_newColor {newLabel}
                {
                    auto& obj = m_path.find();
                    m_oldColor = obj.metadata.color();
                }

                void undo() const override
                {
                    auto& obj = m_path.find();
                    obj.metadata.setColor(m_oldColor);
                }

                void redo() const override
                {
                    auto& obj = m_path.find();
                    obj.metadata.setColor(m_newColor);
                }

            protected:
                virtual void serializeImpl(QDataStream& s) const override
                {
                    s << m_path << m_oldColor << m_newColor;
                }

                virtual void deserializeImpl(QDataStream& s) override
                {
                    s >> m_path >> m_oldColor >> m_newColor;
                }

            private:
                Path<T> m_path;
                QColor m_newColor;
                QColor m_oldColor;
        };
    }
}
