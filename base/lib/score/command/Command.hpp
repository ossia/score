#pragma once
#include <QByteArray>
#include <QString>
#include <score/command/CommandFactoryKey.hpp>

namespace score
{
struct ApplicationContext;
/**
 * @brief The Command class
 *
 * The base of the command system in score
 * It is timestamped, because we can then compare between clients.
 *
 * Maybe the score_plugin_network should replace the Command by a
 * TimestampedCommand instead ?
 * What if other plug-ins also want to add functionality ?
 *
 * Note: for mergeWith put two timestamps, one for the initial command (5 sec)
 * and one for each
 * new command merged.
 *
 * Commands are serializable / deserializable.
 */
class SCORE_LIB_BASE_EXPORT Command
{
public:
  Command();
  virtual ~Command();

  virtual void undo(const score::DocumentContext& ctx) const = 0;
  virtual void redo(const score::DocumentContext& ctx) const = 0;

  virtual const CommandGroupKey& parentKey() const noexcept = 0;
  virtual const CommandKey& key() const noexcept = 0;

  QByteArray serialize() const;
  void deserialize(const QByteArray&);

  virtual QString description() const = 0;

protected:
  virtual void serializeImpl(DataStreamInput&) const = 0;
  virtual void deserializeImpl(DataStreamOutput&) = 0;
  /*
    quint32 timestamp() const;
    void setTimestamp(quint32 stmp);

  private:
    // TODO check if this is UTC
    std::chrono::milliseconds m_timestamp;*/
};
}

/**
 * \macro SCORE_COMMAND_DECL
 * \brief Used to specify the common metadata of commands :
 *
 *  * parentNameFun : An unique identifier for a family of commands,
 *                    for instance all the commands of a given plug-in.
 *  * command name : The name of this command. It must be unique across
 *                   all the commands registered for the previous identifier.
 *  * command description : the text description that will be shown in the UI.
 *
 * It is **mandatory** to use this macro with command classes.
 * This is because the build system scans the source file for these,
 * and produces a file that includes them all and registers them.
 *
 * The CMake code used to achieve this is in the file
 * IScoreFunctions.cmake. See score_generate_command_list_file.
 *
 */
#define SCORE_COMMAND_DECL(parentNameFun, name, desc)        \
public:                                                      \
  name() noexcept                                            \
  {                                                          \
  }                                                          \
  const CommandGroupKey& parentKey() const noexcept override \
  {                                                          \
    return parentNameFun;                                    \
  }                                                          \
  const CommandKey& key() const noexcept override            \
  {                                                          \
    return static_key();                                     \
  }                                                          \
  QString description() const override                       \
  {                                                          \
    return QObject::tr(desc);                                \
  }                                                          \
  static const CommandKey& static_key() noexcept             \
  {                                                          \
    static const CommandKey var{#name};                      \
    return var;                                              \
  }                                                          \
                                                             \
private:

/**
 * \macro SCORE_COMMAND_DECL_T
 * \brief Helper to declare template commands
 *
 * These commands generally have a specific description or key for each
 * instantiation so we just declare the name here.
 */
#define SCORE_COMMAND_DECL_T(name)
