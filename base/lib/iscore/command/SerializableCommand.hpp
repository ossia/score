#pragma once
#include <iscore/command/Command.hpp>

/**
 * This macro is used to specify the common metadata of commands :
 *  - factory name (e.g. "ScenarioControl")
 *  - command name
 *  - command description
 */
#define ISCORE_SERIALIZABLE_COMMAND_DECL(facName, name, desc) \
    public: \
        name (): iscore::SerializableCommand{ factoryName() , commandName(), description() } { } \
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
 * @brief The SerializableCommand class
 *
 * Adds serialization & deserialization capabilities to Command.
 * Most concrete commands shall inherit from this class.
 */
class SerializableCommand : public Command
{
    public:
        ~SerializableCommand();

        const std::string& name() const;
        const std::string& parentName() const; // Note: factoryName() is the constexpr one.
        const QString& text() const;
        void setText(const QString& t);

        SerializableCommand& operator=(const SerializableCommand& other) = default;

        std::size_t uid() const
        {
            std::hash<std::string> fn;
            return fn(this->name());
        }

        QByteArray serialize() const;
        void deserialize(const QByteArray&);

    protected:
        template<typename Str1, typename Str2, typename Str3>
        SerializableCommand(Str1&& parname, Str2&& cmdname, Str3&& text) :
            m_name {cmdname},
            m_parentName {parname},
            m_text{text}
        {
        }


        template<typename T>
        SerializableCommand(const T*) :
            m_name {T::commandName()},
            m_parentName {T::factoryName()},
            m_text{T::description()}
        {
        }

        virtual void serializeImpl(QDataStream&) const = 0;
        virtual void deserializeImpl(QDataStream&) = 0;

    private:
        std::string m_name;
        std::string m_parentName;
        QString m_text;
};
}
