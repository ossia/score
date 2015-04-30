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
            Command(const char* parname, const char* cmdname, QString text) :
                m_name {cmdname},
                m_parentName {parname},
                m_text{text}
            {
            }

            Command(QString parname, QString cmdname, QString text) :
                m_name {cmdname},
                m_parentName {parname},
                m_text{text}
            {
            }

            virtual ~Command() = default;

            QString name() const
            {
                return m_name;
            }

            QString parentName() const
            {
                return m_parentName;
            }

            const QString& text() const
            {
                return m_text;
            }

            void setText(const QString& t)
            {
                m_text = t;
            }

            // A facility for commands that should not be merged.
            // Merging should be explicitely enabled.
            bool canMerge() const
            {
                return m_canMerge;
            }

            void enableMerging()
            {
                m_canMerge = true;
            }

            void disableMerging()
            {
                m_canMerge = false;
            }

            virtual void undo() = 0;
            virtual void redo() = 0;
            virtual bool mergeWith(const Command*) = 0;


            auto uid() const
            {
                using namespace std;
                hash<string> fn;
                return fn(this->name().toStdString());
                /*
                int32_t theUid =
                    hash <= numeric_limits<int32_t>::max() ?
                        static_cast<int32_t>(hash) :
                        static_cast<int32_t>(hash - numeric_limits<int32_t>::max() - 1) + numeric_limits<int32_t>::min();
                return theUid;
                * */
            }


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
            bool m_canMerge {false};
            const QString m_name;
            const QString m_parentName;
            QString m_text;

            //TODO check if this is UTC
            std::chrono::milliseconds m_timestamp
            {  std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now().time_since_epoch()) };
    };
}
