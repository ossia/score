#pragma once
#include <QUndoCommand>
#include <QByteArray>
#include <QDataStream>
#include <QBuffer>
#include <chrono>
#include <string>

#include <QHash>
#include <numeric>

#define ISCORE_COMMAND \
    public: \
    static const char* className(); \
    static QString description(); \
    static auto static_uid() \
    { \
    using namespace std; \
    hash<string> fn; \
    return fn(std::string(className())); \
    } \
    private:

#define ISCORE_COMMAND_DECL(name, desc) \
    public: \
        static const char* className() { return name; } \
        static QString description() { return desc; }  \
    static auto static_uid() \
    { \
    using namespace std; \
    hash<string> fn; \
    return fn(std::string(className())); \
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
            virtual ~Command() = default;

            virtual void undo() = 0;
            virtual void redo() = 0;

        protected:
            quint32 timestamp() const
            {
                return static_cast<quint32>(m_timestamp.count());
            }

            void setTimestamp(quint32 stmp)
            {
                m_timestamp = std::chrono::duration<quint32> (stmp);
            }

        private:
            //TODO check if this is UTC
            std::chrono::milliseconds m_timestamp
            {  std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now().time_since_epoch()) };
    };
}
