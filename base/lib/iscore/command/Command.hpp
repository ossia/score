#pragma once
#include <QByteArray>
#include <QDataStream>
#include <QBuffer>
#include <chrono>
#include <string>

#include <numeric>

#include <iscore/Settings.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

#define ISCORE_COMMAND_DECL_OBSOLETE(name, desc) \
    public: \
        static constexpr const char* commandName() { return name; } \
        static QString description() { return QObject::tr(desc); }  \
    static auto static_uid() \
    { \
        using namespace std; \
        hash<string> fn; \
        return fn(std::string(commandName())); \
    } \
    private:

/**
 * This macro is used to specify the common metadata of commands :
 *  - factory name (e.g. "ScenarioControl")
 *  - command name
 *  - command description
 */
#define ISCORE_COMMAND_DECL(facName, name, desc) \
    public: \
        static constexpr const char* factoryName() { return facName; } \
        static constexpr const char* commandName() { return name; } \
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
     * @brief The Command class
     *
     * The base of the command system in i-score
     * It is timestamped, because we can then compare between clients.
     *
     * Maybe the NetworkPlugin should replace the Command by a TimestampedCommand instead ?
     * What if other plug-ins also want to add functionality ?
     *
     * Note: for mergeWith put two timestamps, one for the initial command (5 sec) and one for each
     * new command merged.
     */
    class Command
    {
        public:
            virtual ~Command();

            virtual void undo() = 0;
            virtual void redo() = 0;

        protected:
            quint32 timestamp() const;
            void setTimestamp(quint32 stmp);

        private:
            //TODO check if this is UTC
            std::chrono::milliseconds m_timestamp
            {  std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now().time_since_epoch()) };
    };
}
