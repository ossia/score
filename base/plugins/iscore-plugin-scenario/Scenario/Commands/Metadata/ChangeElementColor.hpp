#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementColor final : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                const CommandParentFactoryKey& parentKey() const override
                {
                    return ScenarioCommandFactoryName();
                }
                static const CommandFactoryKey& static_key()
                {
                    static const QByteArray name = QString{"ChangeElementColor_%1"}.arg(T::staticMetaObject.className()).toUtf8();
                    static const CommandFactoryKey kagi{name.constData()};
                    return kagi;
                }
                const CommandFactoryKey& key() const override
                {
                    return static_key();
                }
                QString description() const override
                {
                    return QObject::tr("Change %1 color").arg(T::description());
                }

                ChangeElementColor() = default;
                ChangeElementColor(Path<T>&& path, QColor newLabel) :
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
                void serializeImpl(DataStreamInput& s) const override
                {
                    s << m_path << m_oldColor << m_newColor;
                }

                void deserializeImpl(DataStreamOutput& s) override
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
