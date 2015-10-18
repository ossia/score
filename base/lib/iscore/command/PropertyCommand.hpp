#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#define ISCORE_PROPERTY_COMMAND_DECL(facName, name, desc) \
    public: \
        name (): iscore::PropertyCommand{ factoryName() , commandName(), description() } { } \
        static constexpr const char* factoryName() { return facName; } \
        static constexpr const char* commandName() { return #name; } \
        static QString description() { return QObject::tr(desc); }  \
    static auto static_uid() \
    { \
        using namespace std; \
        hash<string> fn; \
        return fn(std::string(commandName())); \
    } \
    private:

namespace iscore
{
/**
 * @brief The PropertyCommand class
 *
 * This generic command allows for a very simple operation when
 * changing a property specified with Q_PROPERTY.
 *
 * It will save the current state and switch between the current and new
 * state upon undo / redo.
 */
class PropertyCommand : public SerializableCommand
{
    public:
        using SerializableCommand::SerializableCommand;
        template<typename Path_T, typename... Args>
        PropertyCommand(Path_T&& path,
                        const QString& property,
                        const QVariant& newval,
                        Args&&... args):
            SerializableCommand{std::forward<Args>(args)...},
            m_path{std::move(std::move(path).moveUnsafePath())},
            m_property{property},
            m_new{newval}
        {
            m_old = m_path.find<QObject>().property(m_property.toUtf8().constData());
        }

        void undo() const override;
        void redo() const override;

        template<typename Path_T>
        void update(const Path_T&, const QVariant& newval)
        {
            m_new = newval;
        }

    protected:
        void serializeImpl(QDataStream &) const override;
        void deserializeImpl(QDataStream &) override;

    private:
        ObjectPath m_path;
        QString m_property;
        QVariant m_old, m_new;
};
}
