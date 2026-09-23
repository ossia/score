#pragma once
#include <score/command/Command.hpp>
#include <score/command/CommandData.hpp>

#include <QObject>
#include <QString>
#include <QTemporaryFile>

#include <vector>

namespace score
{
class CommandStack;
/**
 * @brief Serialized command stack data for backup / restore
 */
struct CommandStackBackup
{
  CommandStackBackup(const score::CommandStack& stack);

  //! Laid out as the stack: the undo commands, then the redo ones, bottom first.
  std::vector<CommandData> savedUndo;
  std::vector<CommandData> savedRedo;
};

/**
 * @brief Abstraction over the backup of commands
 *
 * Synchronizes the commands of a document to an on-disk file,
 * by maintaining serialized stacks of commands at each new command.
 *
 * This way, if there is a crash, the document can be restored from the
 * last successful command and only the latest user action is lost.
 */
class CommandBackupFile final : public QObject
{
public:
  CommandBackupFile(const score::CommandStack& stack, QObject* parent);
  CommandBackupFile(
      const score::CommandStack& stack, const QByteArray& restored, QObject* parent);
  QString fileName() const;

private:
  void init_connections();

  void on_push();
  void on_undo();
  void on_redo();
  void on_indexChanged();

  //! Writes the current buffers to disk.
  void commit();

  const score::CommandStack& m_stack;
  CommandStackBackup m_backup;

#if defined(__EMSCRIPTEN__)
  //! Where the stack lives in local storage: the filesystem of a web page
  //! does not outlive it, which is exactly when a backup has to be readable.
  QString m_key;
#else
  QTemporaryFile m_file;
#endif
};
}
